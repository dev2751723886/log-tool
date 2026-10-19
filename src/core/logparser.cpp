#include "core/logparser.h"
#include <QRegularExpression>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

static void flattenJson(const QJsonObject& o, const QString& prefix, QVariantMap& out, int depth) {
    if (depth > 6) return;
    for (auto it = o.begin(); it != o.end(); ++it) {
        const QString key = prefix.isEmpty() ? it.key() : prefix + "." + it.key();
        const QJsonValue v = it.value();
        if (v.isObject()) flattenJson(v.toObject(), key, out, depth + 1);
        else if (v.isDouble()) out.insert(key, v.toDouble());
        else if (v.isBool()) out.insert(key, v.toBool());
        else if (v.isString()) out.insert(key, v.toString());
        // 数组（如 error_list）忽略
    }
}

// 固定宽度解析时间戳 "yyyy-MM-dd HH:mm:ss.zzz" → epoch 毫秒（比 QDateTime::fromString 快得多）
static inline qint64 parseTimeToEpoch(const QString& s) {
    if (s.size() < 19) return 0;
    int year = s.mid(0, 4).toInt();
    int mon = s.mid(5, 2).toInt();
    int day = s.mid(8, 2).toInt();
    int hh = s.mid(11, 2).toInt();
    int mm = s.mid(14, 2).toInt();
    int ss = s.mid(17, 2).toInt();
    int ms = (s.size() >= 23) ? s.mid(20, 3).toInt() : 0;
    return QDateTime(QDate(year, mon, day), QTime(hh, mm, ss, ms)).toMSecsSinceEpoch();
}

// 解析 key=value 对（value 支持双引号包裹，可含空格）
static void splitKeyValue(const QString& s, QVariantMap& out) {
    int i = 0, n = s.size();
    while (i < n) {
        while (i < n && s[i].isSpace()) i++;
        if (i >= n) break;
        int keyStart = i;
        while (i < n && s[i] != '=' && !s[i].isSpace()) i++;
        QString key = s.mid(keyStart, i - keyStart);
        if (i >= n || s[i] != '=') { i++; continue; }
        i++;  // 跳过 '='
        QString value;
        if (i < n && s[i] == '"') {
            i++;
            int vStart = i;
            while (i < n && s[i] != '"') i++;
            value = s.mid(vStart, i - vStart);
            i++;  // 跳过结束引号
        } else {
            int vStart = i;
            while (i < n && !s[i].isSpace()) i++;
            value = s.mid(vStart, i - vStart);
        }
        out.insert(key, value);
    }
}

// 判断是否为仿真格式 [time] [LEVEL] [module] ...（第 2 个括号是级别关键字）
static bool looksLikeSimulation(const QString& line) {
    int e1 = line.indexOf(']', 1);
    if (e1 < 0 || e1 + 2 >= line.size()) return false;
    if (line[e1 + 1] != ' ' || line[e1 + 2] != '[') return false;
    int e2 = line.indexOf(']', e1 + 3);
    if (e2 < 0) return false;
    QString lv = line.mid(e1 + 3, e2 - (e1 + 3)).trimmed().toUpper();
    return lv == "INFO" || lv == "WARN" || lv == "WARNING" || lv == "ERROR" ||
           lv == "FATAL" || lv == "DEBUG" || lv == "TRACE";
}

// 解析仿真格式：[2026-10-19 09:00:00.000] [INFO] [runner] key=value...
static bool parseSimulation(const QString& line, const QString& sourceFile, LogType type, LogEntry& out) {
    int e1 = line.indexOf(']', 1);
    if (e1 < 0) return false;
    out.timeStr = line.mid(1, e1 - 1);
    int p2 = e1 + 1;
    if (p2 + 1 >= line.size() || line[p2] != ' ' || line[p2 + 1] != '[') return false;
    int e2 = line.indexOf(']', p2 + 2);
    if (e2 < 0) return false;
    out.level = line.mid(p2 + 2, e2 - (p2 + 2)).trimmed().toLower();
    int p3 = e2 + 1;
    if (p3 + 1 >= line.size() || line[p3] != ' ' || line[p3 + 1] != '[') return false;
    int e3 = line.indexOf(']', p3 + 2);
    if (e3 < 0) return false;
    out.node = line.mid(p3 + 2, e3 - (p3 + 2)).trimmed();
    int p4 = e3 + 1;
    if (p4 < line.size() && line[p4] == ' ') p4++;
    out.type = LogType::Simulation;
    out.sourceFile = sourceFile;
    out.epochMs = parseTimeToEpoch(out.timeStr);
    out.message = line.mid(p4);
    splitKeyValue(out.message, out.fields);
    (void)type;
    return true;
}

static QHash<QString, int> s_parseErrors;

void LogParser::resetParseStats() { s_parseErrors.clear(); }
QHash<QString, int> LogParser::parseErrorStats() { return s_parseErrors; }

bool LogParser::parse(const QString& line, const QString& sourceFile, LogType type, LogEntry& out) {
    if (line.isEmpty()) return false;
    if (line[0] == '#') return false;  // 注释行
    if (line[0] != '[') {
        // 通用应用日志格式：2026-10-19 09:00:01 INFO  [Main] message
        static QRegularExpression gre(
            "^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})\\s+(DEBUG|INFO|WARN|WARNING|ERROR|FATAL|TRACE)\\s*\\[([^\\]]+)\\]\\s*(.*)$",
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch gm = gre.match(line);
        if (!gm.hasMatch()) { s_parseErrors[sourceFile]++; return false; }
        out.timeStr = gm.captured(1);
        out.level = gm.captured(2).toLower();
        out.node = gm.captured(3);
        out.type = (type != LogType::Unknown) ? type : LogType::GenericApp;
        out.sourceFile = sourceFile;
        out.epochMs = parseTimeToEpoch(out.timeStr);
        out.message = gm.captured(4);
        return true;
    }

    // 仿真格式：[time] [LEVEL] [module] key=value...
    if (looksLikeSimulation(line)) {
        if (parseSimulation(line, sourceFile, type, out)) return true;
        s_parseErrors[sourceFile]++;
        return false;
    }

    // 时间 [yyyy-MM-dd HH:mm:ss.mmm]
    int e1 = line.indexOf(']', 1);
    if (e1 < 0) return false;
    out.timeStr = line.mid(1, e1 - 1);

    // 节点 [node]
    int p2 = e1 + 1;
    if (p2 + 1 >= line.size() || line[p2] != ' ' || line[p2 + 1] != '[') return false;
    int e2 = line.indexOf(']', p2 + 2);
    if (e2 < 0) return false;
    out.node = line.mid(p2 + 2, e2 - (p2 + 2));

    // 级别 [level]
    int p3 = e2 + 1;
    if (p3 + 1 >= line.size() || line[p3] != ' ' || line[p3 + 1] != '[') return false;
    int e3 = line.indexOf(']', p3 + 2);
    if (e3 < 0) return false;
    out.level = line.mid(p3 + 2, e3 - (p3 + 2));

    int p4 = e3 + 1;
    if (p4 < line.size() && line[p4] == ' ') p4++;
    QString rest = line.mid(p4);

    out.type = (type != LogType::Unknown) ? type : detectLogType(out.node);
    out.sourceFile = sourceFile;
    out.epochMs = parseTimeToEpoch(out.timeStr);

    // 提取 [file:line]
    if (rest.size() > 2 && rest[0] == '[') {
        int fe = rest.indexOf(']');
        if (fe > 0) {
            QString fl = rest.mid(1, fe - 1);
            int dot = fl.indexOf('.');
            int colon = fl.lastIndexOf(':');
            if (dot > 0 && colon > dot && colon < fl.size() - 1) {
                bool digits = true;
                for (int i = colon + 1; i < fl.size(); ++i)
                    if (!fl[i].isDigit()) { digits = false; break; }
                if (digits) {
                    out.fileLine = fl;
                    rest = rest.mid(fe + 1);
                    if (rest.startsWith(' ')) rest = rest.mid(1);
                }
            }
        }
    }

    const int MAX_MSG = 8000;
    out.message = rest.size() > MAX_MSG ? rest.left(MAX_MSG) : rest;

    enrich(out);
    return true;
}

void LogParser::enrich(LogEntry& e) {
    const QString& msg = e.message;
    switch (e.type) {
    case LogType::BtStatus: {
        if (msg.indexOf("BT Logger") < 0) break;
        static QRegularExpression re(
            "node name:([^,]+)\\s*, previous state:\\[([^\\]]*)\\], current state:\\[([^\\]]*)\\]");
        auto m = re.match(msg);
        if (m.hasMatch()) {
            e.fields["bt_node"] = m.captured(1).trimmed();
            e.fields["bt_prev"] = m.captured(2);
            e.fields["bt_cur"] = m.captured(3);
        }
        break;
    }
    case LogType::MotionControl: {
        int b = msg.indexOf('{');
        if (b < 0) break;
        QJsonDocument doc = QJsonDocument::fromJson(msg.mid(b).toUtf8());
        if (!doc.isObject()) break;
        QJsonObject root = doc.object();
        e.fields["data_type"] = root.value("data_type").toString();
        e.fields["msg_index"] = root.value("msg_index").toDouble();
        QJsonObject data = root.value("data").toObject();
        flattenJson(data, "", e.fields, 0);
        break;
    }
    case LogType::PlannerControl: {
        if (msg.indexOf("cs:") >= 0) {
            static QRegularExpression pos(
                "x:\\s*([-+\\d.eE]+),\\s*y:\\s*([-+\\d.eE]+),\\s*h:\\s*([-+\\d.eE]+),\\s*v:\\s*([-+\\d.eE]+),\\s*omega:\\s*([-+\\d.eE]+),\\s*cs:\\s*(-?\\d+)");
            auto m = pos.match(msg);
            if (m.hasMatch()) {
                e.fields["x"] = m.captured(1).toDouble();
                e.fields["y"] = m.captured(2).toDouble();
                e.fields["h"] = m.captured(3).toDouble();
                e.fields["v"] = m.captured(4).toDouble();
                e.fields["omega"] = m.captured(5).toDouble();
                e.fields["cs"] = m.captured(6).toDouble();
            }
        }
        if (msg.indexOf("control mode changed") >= 0) {
            static QRegularExpression cm("control mode changed,\\s*cur:(-?\\d+),\\s*pre:(-?\\d+)");
            auto m = cm.match(msg);
            if (m.hasMatch()) { e.fields["ctrl_mode_cur"] = m.captured(1).toDouble(); e.fields["ctrl_mode_pre"] = m.captured(2).toDouble(); }
        }
        if (msg.indexOf("controller state changed") >= 0) {
            static QRegularExpression cs("controller state changed,\\s*cur:(-?\\d+),\\s*pre:(-?\\d+)");
            auto m = cs.match(msg);
            if (m.hasMatch()) { e.fields["ctrl_state_cur"] = m.captured(1).toDouble(); e.fields["ctrl_state_pre"] = m.captured(2).toDouble(); }
        }
        if (msg.indexOf("not in running") >= 0) {
            static QRegularExpression st("not in running controller state:(-?\\d+)");
            auto m = st.match(msg);
            if (m.hasMatch()) e.fields["stuck_state"] = m.captured(1).toDouble();
        }
        break;
    }
    case LogType::NavDefault: {
        if (msg.indexOf("error_code=") >= 0) {
            static QRegularExpression ec("error_code=([A-Za-z0-9]+)");
            auto m = ec.match(msg); if (m.hasMatch()) e.fields["error_code"] = m.captured(1);
        }
        if (msg.indexOf("pos_confidence=") >= 0) {
            static QRegularExpression pc("pos_confidence=([\\d.]+)");
            auto m = pc.match(msg); if (m.hasMatch()) e.fields["pos_confidence"] = m.captured(1).toDouble();
        }
        if (msg.indexOf("location_state=") >= 0) {
            static QRegularExpression ls("location_state=(\\d+)");
            auto m = ls.match(msg); if (m.hasMatch()) e.fields["location_state"] = m.captured(1).toDouble();
        }
        if (msg.indexOf("required>") >= 0) {
            static QRegularExpression rq("required>([\\d.]+)");
            auto m = rq.match(msg); if (m.hasMatch()) e.fields["required_confidence"] = m.captured(1).toDouble();
        }
        break;
    }
    case LogType::OaDetect: {
        if (msg.indexOf("obstacle exist") >= 0) {
            static QRegularExpression oe("obstacle exist:\\s*(true|false)");
            auto m = oe.match(msg); if (m.hasMatch()) e.fields["obstacle_exist"] = m.captured(1);
        }
        if (msg.indexOf("controller_state") >= 0) {
            static QRegularExpression cs("controller_state:\\s*(\\d+)");
            auto m = cs.match(msg); if (m.hasMatch()) e.fields["oa_controller_state"] = m.captured(1).toDouble();
        }
        if (msg.indexOf("oa_status") >= 0) {
            static QRegularExpression os("oa_status:(\\d+)");
            auto m = os.match(msg); if (m.hasMatch()) e.fields["oa_status"] = m.captured(1).toDouble();
        }
        break;
    }
    default:
        break;
    }
}
