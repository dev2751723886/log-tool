#include "store/reportwriter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QHash>
#include <QSet>

QString ReportWriter::toJson(const QVector<Anomaly>& anomalies) {
    QJsonArray arr;
    for (const Anomaly& a : anomalies) {
        QJsonObject o;
        o["type"] = issueTypeName(a.type);
        o["severity"] = severityName(a.severity);
        o["time"] = a.timeStr;
        o["title"] = a.title;
        o["description"] = a.description;
        o["count"] = a.count;
        QJsonArray causes, sols;
        for (const QString& c : a.causes) causes.append(c);
        for (const QString& s : a.solutions) sols.append(s);
        o["causes"] = causes;
        o["solutions"] = sols;
        arr.append(o);
    }
    QJsonDocument doc(arr);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool ReportWriter::save(const QVector<Anomaly>& anomalies, const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    f.write(toJson(anomalies).toUtf8());
    f.close();
    return true;
}

bool ReportWriter::load(const QString& path, QVector<Anomaly>& out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray data = f.readAll();
    f.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return false;
    out.clear();
    for (const QJsonValue& v : doc.array()) {
        QJsonObject o = v.toObject();
        Anomaly a;
        a.type = issueTypeFromName(o.value("type").toString());
        a.severity = severityFromName(o.value("severity").toString());
        a.timeStr = o.value("time").toString();
        a.title = o.value("title").toString();
        a.description = o.value("description").toString();
        a.count = o.value("count").toInt(1);
        for (const QJsonValue& c : o.value("causes").toArray()) a.causes << c.toString();
        for (const QJsonValue& s : o.value("solutions").toArray()) a.solutions << s.toString();
        out.append(a);
    }
    return true;
}

static QString anomalyKey(const Anomaly& a) {
    return QString("%1|%2").arg(issueTypeName(a.type), a.title);
}

ReportWriter::CompareResult ReportWriter::compare(const QVector<Anomaly>& before,
                                                  const QVector<Anomaly>& after) {
    CompareResult r;
    QSet<QString> bKeys, aKeys;
    for (const Anomaly& a : before) bKeys.insert(anomalyKey(a));
    for (const Anomaly& a : after) aKeys.insert(anomalyKey(a));
    for (const Anomaly& a : before) {
        if (aKeys.contains(anomalyKey(a))) r.persistent.append(a);
        else r.resolved.append(a);
    }
    for (const Anomaly& a : after) {
        if (!bKeys.contains(anomalyKey(a))) r.added.append(a);
    }
    return r;
}

QString ReportWriter::summary(const QVector<Anomaly>& anomalies) {
    QHash<int, int> cnt, sev;
    for (const Anomaly& a : anomalies) { cnt[(int)a.type]++; sev[(int)a.severity]++; }
    QString s = QString("共发现 %1 个问题：\n").arg(anomalies.size());
    for (auto it = cnt.begin(); it != cnt.end(); ++it)
        s += QString("  - %1：%2 处\n").arg(issueTypeName((IssueType)it.key())).arg(it.value());
    s += QString("严重度分布：严重 %1 / 错误 %2 / 警告 %3 / 提示 %4")
            .arg(sev[(int)Severity::Critical]).arg(sev[(int)Severity::Error])
            .arg(sev[(int)Severity::Warning]).arg(sev[(int)Severity::Info]);
    return s;
}

QString ReportWriter::toHtml(const QVector<Anomaly>& anomalies,
                             const QVector<RunSummary>& runs,
                             const QVector<ConstraintCheck>& constraints) {
    QString h;
    h += "<html><head><meta charset='utf-8'><style>"
         "body{font-family:'Microsoft YaHei',sans-serif;margin:20px}"
         "table{border-collapse:collapse;width:100%;margin-bottom:18px}"
         "th,td{border:1px solid #ccc;padding:6px;text-align:left;font-size:13px}"
         "th{background:#f0f0f0}"
         ".PASS{color:#0E9F6E;font-weight:bold}.FAIL{color:#E5484D;font-weight:bold}"
         ".WARNING{color:#F59E0B;font-weight:bold}"
         "</style></head><body><h1>仿真运行报告</h1>";

    h += "<h2>运行汇总</h2><table><tr><th>运行</th><th>模型</th><th>结论</th><th>错误</th><th>警告</th><th>失败原因</th></tr>";
    for (const RunSummary& r : runs) {
        h += QString("<tr><td>%1</td><td>%2</td><td class='%3'>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
                .arg(r.runId, r.model, r.verdict).arg(r.errorCount).arg(r.warnCount)
                .arg(r.failReason.toHtmlEscaped());
    }
    h += "</table>";

    h += "<h2>约束/断言检查（不满足项）</h2><table><tr><th>运行</th><th>约束</th><th>期望</th><th>实际</th><th>结果</th></tr>";
    for (const ConstraintCheck& c : constraints) {
        h += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td style='color:#E5484D'>✗ 不满足</td></tr>")
                .arg(c.runId, c.name, c.expected.toHtmlEscaped(), c.actual.toHtmlEscaped());
    }
    h += "</table>";

    h += "<h2>错误定位</h2><table><tr><th>时间</th><th>类型</th><th>文件/函数</th><th>行号</th><th>描述</th></tr>";
    for (const Anomaly& a : anomalies) {
        if (a.timeStr.isEmpty()) continue;
        QString type = a.errorType.isEmpty() ? issueTypeName(a.type) : a.errorType;
        QString desc = a.description;
        if (desc.size() > 120) desc = desc.left(120) + "...";
        h += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                .arg(a.timeStr, type, a.file.toHtmlEscaped()).arg(a.line).arg(desc.toHtmlEscaped());
    }
    h += "</table></body></html>";
    return h;
}

bool ReportWriter::saveHtml(const QString& path,
                            const QVector<Anomaly>& anomalies,
                            const QVector<RunSummary>& runs,
                            const QVector<ConstraintCheck>& constraints) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    f.write(toHtml(anomalies, runs, constraints).toUtf8());
    f.close();
    return true;
}
