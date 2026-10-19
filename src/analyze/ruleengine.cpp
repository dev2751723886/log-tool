#include "analyze/ruleengine.h"
#include "analyze/knowledgebase.h"
#include "core/logparser.h"
#include <QRegularExpression>
#include <QHash>
#include <algorithm>

// 行为树节点分类：区分结构/调试节点、条件/检查节点（失败属正常语义）与动作节点（失败为真错误）
static bool isBtStructural(const QString& n) {
    // 结构/复合节点（含 SequenceStar、ChassisSequence、GyroZeroIfThenElse 等自定义组合节点）
    return n.contains("Sequence") || n.contains("Fallback") || n.contains("Parallel") ||
           n.contains("IfThenElse") || n.contains("Reactive") || n.contains("Selector") ||
           n.contains("Decorator") ||
           n == "AlwaysFailure" || n == "AlwaysSuccess";
}
static bool isBtCondition(const QString& n) {
    return n.contains("Check") || n.contains("Condition") || n.contains("Effective") ||
           n.contains("Is") || n.contains("Exist") || n.contains("Valid") || n.contains("Compare");
}

QVector<LogEntry> RuleEngine::contextWindow(const Timeline& tl, qint64 t,
                                            qint64 beforeMs, qint64 afterMs, int max) const {
    QVector<int> idx = tl.range(t - beforeMs, t + afterMs);
    QVector<LogEntry> out;
    int step = idx.isEmpty() ? 1 : (idx.size() / max + 1);
    for (int i = 0; i < idx.size() && out.size() < max; i += step) {
        out.append(tl.entries()[idx[i]]);
    }
    return out;
}

void RuleEngine::detectCrashes(const Timeline& tl, QVector<Anomaly>& out) {
    QRegularExpression crash(
        "segmentation fault|segfault|core dump|assertion|terminate called|SIGSEGV|SIGABRT|stack trace|bad_alloc|what\\(\\):"
        "|nullpointerexception|outofmemoryerror|stackoverflowerror|classcastexception|indexoutofboundsexception|arithmeticexception|fatal error",
        QRegularExpression::CaseInsensitiveOption);
    for (const LogEntry& e : tl.entries()) {
        if (e.type == LogType::Simulation) continue;  // 仿真崩溃由 detectSimulationIssues 处理
        bool fatal = e.level.compare("fatal", Qt::CaseInsensitive) == 0;
        bool kw = crash.match(e.message).hasMatch();
        if (!fatal && !kw) continue;
        Anomaly a;
        a.type = IssueType::Crash;
        a.severity = Severity::Critical;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = "检测到程序崩溃 / 致命错误";
        a.description = QString("节点 %1（%2）出现致命信号或崩溃特征：\n%3")
                .arg(e.node, e.fileLine, e.message.left(400));
        a.relatedFiles << e.sourceFile;
        a.context = contextWindow(tl, e.epochMs, 3000, 2000, 200);
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::detectNodeFailures(const Timeline& tl, QVector<Anomaly>& out) {
    QHash<QString, int> lastIdx;
    const qint64 BUCKET = 10000;  // 10s 聚合，去重重试循环
    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::BtStatus) continue;
        if (e.str("bt_cur") != "FAILURE") continue;
        QString node = e.str("bt_node");
        if (node.isEmpty()) node = "unknown";
        // 结构性/调试节点失败属正常语义，跳过
        if (isBtStructural(node)) continue;
        // 条件/检查类节点失败属正常语义（行为树分支路由），跳过
        if (isBtCondition(node)) continue;

        IssueType it = (node == "RetryUntilSuccessful") ? IssueType::ChassisControlTimeout
                                                        : IssueType::NodeExecutionFailure;
        qint64 bucket = e.epochMs / BUCKET;
        QString key = node + "|" + QString::number(bucket);
        if (lastIdx.contains(key)) {
            Anomaly& a = out[lastIdx[key]];
            a.count++;
            a.timeMs = qMax(a.timeMs, e.epochMs);
            if (a.context.size() < 200) a.context.append(e);
            continue;
        }
        Anomaly a;
        a.type = it;
        a.severity = (node == "RetryUntilSuccessful") ? Severity::Warning : Severity::Error;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = (node == "RetryUntilSuccessful")
                ? QString("底盘控制重试耗尽：%1").arg(node)
                : QString("行为树动作节点执行失败：%1").arg(node);
        a.description = QString("节点 %1 在 %2 进入 FAILURE（%3 → FAILURE）。")
                .arg(node, e.timeStr, e.str("bt_prev"));
        if (node == "RetryUntilSuccessful")
            a.description += " 重试直至成功的节点已耗尽重试次数，说明被重试的动作多次失败（疑似底盘/IO 无响应）。";
        a.relatedFiles << e.sourceFile;
        a.context = contextWindow(tl, e.epochMs, 5000, 3000, 200);
        lastIdx[key] = out.size();
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::detectSensorLoss(const Timeline& tl, QVector<Anomaly>& out) {
    // 1) 定位置信度不达标（nav_default 报 Check failed）
    QHash<QString, int> seen;
    const qint64 BUCKET = 10000;
    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::NavDefault) continue;
        if (e.level.compare("error", Qt::CaseInsensitive) != 0) continue;
        if (!e.has("pos_confidence")) continue;
        double pc = e.num("pos_confidence");
        double req = e.has("required_confidence") ? e.num("required_confidence") : 99.5;
        if (pc >= req) continue;
        qint64 bucket = e.epochMs / BUCKET;
        QString key = "loc|" + QString::number(bucket);
        if (seen.contains(key)) { out[seen[key]].count++; continue; }
        Anomaly a;
        a.type = IssueType::SensorDataLoss;
        a.severity = Severity::Error;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = "定位数据丢失 / 置信度不足";
        a.description = QString("定位置信度检查失败：pos_confidence=%1，要求>%2（%3）。位置数据不可用，可能导致定位丢失。")
                .arg(pc).arg(req).arg(e.timeStr);
        a.relatedFiles << e.sourceFile;
        a.context = contextWindow(tl, e.epochMs, 5000, 5000, 200);
        seen[key] = out.size();
        KnowledgeBase::apply(a);
        out.append(a);
    }
    // 2) MCU 反馈数据中断（帧间隔 > 5s，正常约 50ms）
    const QVector<int>& mc = tl.byType(LogType::MotionControl);
    for (int i = 1; i < mc.size(); ++i) {
        const LogEntry& prev = tl.entries()[mc[i - 1]];
        const LogEntry& cur = tl.entries()[mc[i]];
        qint64 dt = cur.epochMs - prev.epochMs;
        if (dt <= 5000) continue;
        Anomaly a;
        a.type = IssueType::SensorDataLoss;
        a.severity = Severity::Warning;
        a.timeMs = cur.epochMs; a.timeStr = cur.timeStr;
        a.title = "MCU 传感器反馈中断";
        a.description = QString("motion_control 反馈帧中断 %1 秒（%2 → %3），期间传感器 / 里程计数据缺失。")
                .arg(dt / 1000.0).arg(prev.timeStr, cur.timeStr);
        a.relatedFiles << cur.sourceFile;
        a.context = contextWindow(tl, cur.epochMs, 3000, 3000, 100);
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::detectChassisTimeout(const Timeline& tl, QVector<Anomaly>& out) {
    QHash<QString, int> seen;
    const qint64 BUCKET = 30000;
    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::PlannerControl) continue;
        if (!e.has("stuck_state")) continue;
        qint64 bucket = e.epochMs / BUCKET;
        QString key = "stuck|" + QString::number(bucket);
        if (seen.contains(key)) { out[seen[key]].count++; continue; }
        Anomaly a;
        a.type = IssueType::ChassisControlTimeout;
        a.severity = Severity::Warning;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = "底盘控制器未进入运行态";
        a.description = QString("planner_control 持续报“not in running controller state:%1”，底盘控制器卡在非运行态，运动指令无法执行。")
                .arg(e.num("stuck_state"), 0, 'f', 0);
        a.relatedFiles << e.sourceFile;
        a.context = contextWindow(tl, e.epochMs, 5000, 5000, 100);
        seen[key] = out.size();
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::detectInterfaceErrors(const Timeline& tl, QVector<Anomaly>& out) {
    QHash<QString, int> seen;
    const qint64 BUCKET = 60000;
    for (const LogEntry& e : tl.entries()) {
        if (e.level.compare("error", Qt::CaseInsensitive) != 0) continue;
        if (e.type == LogType::Simulation) continue;  // 仿真错误由 detectSimulationIssues 处理
        if (e.has("pos_confidence")) continue;  // 已归入传感器丢失
        QString sig = e.str("error_code");
        if (sig.isEmpty()) sig = e.node + ":" + e.message.left(60);
        qint64 bucket = e.epochMs / BUCKET;
        QString key = sig + "|" + QString::number(bucket);
        if (seen.contains(key)) { out[seen[key]].count++; continue; }
        Anomaly a;
        a.type = IssueType::InterfaceError;
        a.severity = Severity::Error;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = e.has("error_code") ? QString("接口返回错误：%1").arg(sig) : "接口返回错误";
        a.description = QString("%1 报错（%2）：\n%3").arg(e.node, e.fileLine, e.message.left(400));
        if (e.has("error_code")) a.description += QString("\n错误码：%1").arg(sig);
        a.relatedFiles << e.sourceFile;
        a.context = contextWindow(tl, e.epochMs, 5000, 3000, 100);
        seen[key] = out.size();
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::crossValidate(const Timeline& tl, QVector<Anomaly>& out) {
    // 核心跨文件印证：planner 下发速度指令 v>0，但随后电机/里程计反馈仍为 0
    const QVector<int>& pl = tl.byType(LogType::PlannerControl);
    for (int idx : pl) {
        const LogEntry& e = tl.entries()[idx];
        if (!e.has("v")) continue;
        if (e.num("v") <= 0.001) continue;
        bool moved = false;
        QVector<int> window = tl.range(e.epochMs, e.epochMs + 5000);
        for (int wi : window) {
            const LogEntry& m = tl.entries()[wi];
            if (m.type != LogType::MotionControl) continue;
            double rpm = m.num("chassis.chassis_model.drive_wheel.motor_driver.rpm");
            double vel = m.num("chassis.location.odom.vel_x");
            if (qAbs(rpm) > 0.5 || qAbs(vel) > 0.5) { moved = true; break; }
        }
        if (moved) continue;
        Anomaly a;
        a.type = IssueType::CrossValidationMismatch;
        a.severity = Severity::Warning;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = "有运动指令但电机无反馈";
        a.description = QString("planner_control 在 %1 下发速度指令 v=%2，但随后 5 秒内 motion_control 的电机转速 / 里程计速度始终为 0。控制流程完成，实际运动未发生。")
                .arg(e.timeStr).arg(e.num("v"));
        a.relatedFiles << e.sourceFile;
        a.context = contextWindow(tl, e.epochMs, 3000, 6000, 200);
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::detectSimulationIssues(const Timeline& tl, QVector<Anomaly>& out) {
    QHash<QString, int> seen;
    auto emitIssue = [&](const LogEntry& e, IssueType type, Severity sev,
                         const QString& title, const QString& desc) {
        QString k = QString("%1|%2").arg((int)type).arg(e.str("run_id"));
        if (seen.contains(k)) { out[seen[k]].count++; return; }
        Anomaly a;
        a.type = type; a.severity = sev;
        a.timeMs = e.epochMs; a.timeStr = e.timeStr;
        a.title = title; a.description = desc;
        a.relatedFiles << e.sourceFile;
        a.errorType = e.str("error");
        a.file = !e.str("function").isEmpty() ? e.str("function") : e.str("file");
        a.line = e.lineNumber;
        a.context = contextWindow(tl, e.epochMs, 5000, 3000, 60);
        KnowledgeBase::apply(a);
        seen[k] = out.size();
        out.append(a);
    };

    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::Simulation) continue;
        QString lv = e.level.toLower();
        QString err = e.str("error");
        QString warn = e.str("warning");
        QString ev = e.str("event");
        QString st = e.str("status");
        QString reason = e.str("reason");
        QString run = e.str("run_id");

        // 案例2 运行时崩溃
        if (lv == "fatal" || err == "ACCESS_VIOLATION" || ev == "ABORT" || e.has("stack")) {
            emitIssue(e, IssueType::Crash, Severity::Critical,
                      QString("仿真运行时崩溃（%1）").arg(run),
                      QString("模块 %1 报致命错误：%2").arg(e.node, e.message.left(200)));
            continue;
        }
        // 案例3 数值 NaN/Inf
        if (err == "NON_FINITE_VALUE" || warn == "STATE_JUMP" ||
            e.str("value") == "NaN" || e.str("value") == "Inf" || e.str("value") == "-Inf") {
            emitIssue(e, IssueType::NumericalError, Severity::Error,
                      QString("数值异常（%1）").arg(run),
                      QString("模块 %1 出现非有限数值：%2").arg(e.node, e.message.left(200)));
            continue;
        }
        // 案例4 求解器不收敛
        if ((ev == "CONVERGENCE" && st == "FAILED") || warn == "RESIDUAL_INCREASE") {
            emitIssue(e, IssueType::NonConvergence, Severity::Error,
                      QString("求解器不收敛（%1）").arg(run),
                      QString("模块 %1 求解未收敛：%2").arg(e.node, e.message.left(200)));
            continue;
        }
        // 案例5 跨模块数据不一致
        if (warn == "VALUE_MISMATCH" || (e.has("expected") && e.has("actual"))) {
            emitIssue(e, IssueType::DataInconsistency, Severity::Error,
                      QString("跨模块数据不一致（%1）").arg(run),
                      QString("模块 %1 数据不一致：%2").arg(e.node, e.message.left(200)));
            continue;
        }
        // 案例6 性能超时
        if (ev == "TIMEOUT" || warn == "SLOW_STAGE" || reason == "TIMEOUT_RETRY_EXHAUSTED") {
            emitIssue(e, IssueType::PerformanceTimeout, Severity::Warning,
                      QString("性能超时（%1）").arg(run),
                      QString("模块 %1 超时/过慢：%2").arg(e.node, e.message.left(200)));
            continue;
        }
        // 案例7 配置/输入错误
        if (err == "PARAMETER_OUT_OF_RANGE" || err == "FILE_NOT_FOUND" ||
            warn == "UNRECOGNIZED_VALUE" || reason == "INVALID_INPUT") {
            emitIssue(e, IssueType::ConfigError, Severity::Error,
                      QString("配置/输入错误（%1）").arg(run),
                      QString("模块 %1 配置或输入错误：%2").arg(e.node, e.message.left(200)));
            continue;
        }
        // 案例8 基准结果偏差
        if (err == "BASELINE_DEVIATION") {
            emitIssue(e, IssueType::RegressionDeviation, Severity::Error,
                      QString("基准结果偏差（%1）").arg(run),
                      QString("指标 %1 偏离基准：%2").arg(e.str("metric"), e.message.left(200)));
            continue;
        }
        // 断言失败（Assertion failed / assert / CHECK failed）
        if (e.message.contains("assert", Qt::CaseInsensitive) ||
            err == "ASSERTION_FAILED" ||
            e.message.contains("check failed", Qt::CaseInsensitive)) {
            emitIssue(e, IssueType::DataInconsistency, Severity::Error,
                      QString("断言失败（%1）").arg(run),
                      QString("验证条件不满足：%2").arg(e.message.left(200)));
            continue;
        }
    }

    detectTimestampAnomaly(tl, out);

    // 案例10 格式错误
    const QHash<QString, int> parseErrs = LogParser::parseErrorStats();
    for (auto it = parseErrs.begin(); it != parseErrs.end(); ++it) {
        if (it.value() <= 0) continue;
        Anomaly a;
        a.type = IssueType::ParseError;
        a.severity = Severity::Warning;
        a.timeMs = 0; a.timeStr = "";
        a.title = QString("日志格式错误（%1 行无法解析）").arg(it.value());
        a.description = QString("文件 %1 中有 %2 行无法解析（时间戳格式不一致或字段缺失），已跳过但未中断扫描。")
                .arg(it.key()).arg(it.value());
        a.relatedFiles << it.key();
        KnowledgeBase::apply(a);
        out.append(a);
    }
}

void RuleEngine::detectTimestampAnomaly(const Timeline& tl, QVector<Anomaly>& out) {
    // 按文件分组，按原始行号排序，检测时间倒退与重复事件
    QHash<QString, QVector<const LogEntry*>> byFile;
    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::Simulation) continue;
        byFile[e.sourceFile].append(&e);
    }
    for (auto it = byFile.begin(); it != byFile.end(); ++it) {
        QVector<const LogEntry*>& list = it.value();
        std::sort(list.begin(), list.end(), [](const LogEntry* a, const LogEntry* b) {
            return a->lineNumber < b->lineNumber;
        });
        const LogEntry* prev = nullptr;
        for (const LogEntry* e : list) {
            if (prev) {
                if (e->epochMs < prev->epochMs) {
                    Anomaly a;
                    a.type = IssueType::TimestampAnomaly;
                    a.severity = Severity::Warning;
                    a.timeMs = e->epochMs; a.timeStr = e->timeStr;
                    a.title = "时间戳倒退";
                    a.description = QString("时间戳倒退：%1 早于上一行 %2（行 %3 → %4）。")
                            .arg(e->timeStr, prev->timeStr).arg(prev->lineNumber).arg(e->lineNumber);
                    a.relatedFiles << e->sourceFile;
                    a.context = contextWindow(tl, e->epochMs, 2000, 2000, 40);
                    KnowledgeBase::apply(a);
                    out.append(a);
                }
                if (!e->str("event").isEmpty() && e->str("event") == prev->str("event") &&
                    e->str("step") == prev->str("step") && e->epochMs == prev->epochMs) {
                    Anomaly a;
                    a.type = IssueType::TimestampAnomaly;
                    a.severity = Severity::Warning;
                    a.timeMs = e->epochMs; a.timeStr = e->timeStr;
                    a.title = "重复事件";
                    a.description = QString("重复事件：event=%1 step=%2 连续出现两次（行 %3、%4）。")
                            .arg(e->str("event"), e->str("step")).arg(prev->lineNumber).arg(e->lineNumber);
                    a.relatedFiles << e->sourceFile;
                    a.context = contextWindow(tl, e->epochMs, 2000, 2000, 40);
                    KnowledgeBase::apply(a);
                    out.append(a);
                }
            }
            prev = e;
        }
    }
}

void RuleEngine::computeRunSummaries(const Timeline& tl) {
    m_runs.clear();
    QHash<QString, RunSummary> map;
    QHash<QString, bool> hasFinish;
    QHash<QString, int> exitCode;
    QHash<QString, QString> finishStatus;
    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::Simulation) continue;
        QString run = e.str("run_id");
        if (run.isEmpty()) continue;
        RunSummary& r = map[run];
        if (r.runId.isEmpty()) { r.runId = run; r.startMs = e.epochMs; }
        r.endMs = e.epochMs;
        if (!e.str("model").isEmpty()) r.model = e.str("model");
        QString lv = e.level.toLower();
        if (lv == "error" || lv == "fatal") r.errorCount++;
        else if (lv == "warn" || lv == "warning") r.warnCount++;
        if (e.str("event") == "FINISH") {
            hasFinish[run] = true;
            finishStatus[run] = e.str("status");
            QString ec = e.str("exit_code");
            if (!ec.isEmpty()) exitCode[run] = ec.toInt();
        }
    }
    for (auto it = map.begin(); it != map.end(); ++it) {
        RunSummary r = it.value();
        if (r.errorCount > 0) { r.verdict = "FAIL"; r.failReason = QString("含 %1 个错误").arg(r.errorCount); }
        else if (!hasFinish.value(r.runId)) { r.verdict = "FAIL"; r.failReason = "未到达 FINISH"; }
        else if (exitCode.value(r.runId, 0) != 0) { r.verdict = "FAIL"; r.failReason = QString("退出码 %1").arg(exitCode.value(r.runId)); }
        else if (finishStatus.value(r.runId) == "FAILED") { r.verdict = "FAIL"; r.failReason = "结束状态 FAILED"; }
        else if (r.warnCount > 0) { r.verdict = "WARNING"; }
        else { r.verdict = "PASS"; }
        m_runs.append(r);
    }
    std::sort(m_runs.begin(), m_runs.end(), [](const RunSummary& a, const RunSummary& b) {
        return a.runId < b.runId;
    });
}

static double pctToDouble(const QString& s) {
    QString t = s.trimmed();
    t.remove('%');
    bool ok = false;
    double v = t.toDouble(&ok);
    return ok ? v : -1.0;
}

void RuleEngine::computeConstraints(const Timeline& tl) {
    m_constraints.clear();
    for (const LogEntry& e : tl.entries()) {
        if (e.type != LogType::Simulation) continue;
        QString run = e.str("run_id");
        QString err = e.str("error");
        QString warn = e.str("warning");
        QString ev = e.str("event");
        QString st = e.str("status");

        if (ev == "CONVERGENCE" && st == "FAILED") {
            m_constraints.append({run, "残差 < 容差", "residual < " + e.str("tolerance"),
                                  "residual=" + e.str("residual"), false, e.message});
        }
        if (err == "NON_FINITE_VALUE" || e.str("value") == "NaN" || e.str("value") == "Inf") {
            m_constraints.append({run, "数值有限（无 NaN/Inf）", "变量值为有限数",
                                  e.str("variable") + "=" + e.str("value"), false, e.message});
        }
        if (ev == "TIMEOUT") {
            m_constraints.append({run, "耗时 < 阈值", "elapsed < " + e.str("timeout_ms") + "ms",
                                  "elapsed=" + e.str("elapsed_ms") + "ms", false, e.message});
        }
        if (e.has("rel_diff") && e.has("tolerance")) {
            double rel = pctToDouble(e.str("rel_diff"));
            double tol = pctToDouble(e.str("tolerance"));
            if (rel >= 0 && tol >= 0 && rel > tol) {
                m_constraints.append({run, "指标偏离 < 容差", "偏离 < " + e.str("tolerance"),
                                      e.str("metric") + " 偏离 " + e.str("rel_diff"), false, e.message});
            }
        }
        if (err == "PARAMETER_OUT_OF_RANGE") {
            m_constraints.append({run, "参数在约束范围内", e.str("constraint"),
                                  e.str("parameter") + "=" + e.str("value"), false, e.message});
        }
        if (err == "FILE_NOT_FOUND") {
            m_constraints.append({run, "依赖文件存在", "文件可访问", "缺失 " + e.str("file"), false, e.message});
        }
        if (e.has("expected") && e.has("actual")) {
            m_constraints.append({run, "模块数据一致", "expected == actual",
                                  "expected=" + e.str("expected") + " actual=" + e.str("actual"), false, e.message});
        }
    }
}

QVector<Anomaly> RuleEngine::analyze(const Timeline& tl) {
    QVector<Anomaly> out;
    detectCrashes(tl, out);
    detectNodeFailures(tl, out);
    detectSensorLoss(tl, out);
    detectChassisTimeout(tl, out);
    detectInterfaceErrors(tl, out);
    crossValidate(tl, out);
    detectSimulationIssues(tl, out);
    computeRunSummaries(tl);
    computeConstraints(tl);
    std::sort(out.begin(), out.end(), [](const Anomaly& a, const Anomaly& b) {
        if ((int)a.severity != (int)b.severity) return (int)a.severity < (int)b.severity;
        return a.timeMs < b.timeMs;
    });
    return out;
}
