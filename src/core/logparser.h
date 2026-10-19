#pragma once
#include "core/logentry.h"
#include <QString>
#include <QHash>

// 日志解析：统一头部 + 各类型结构化字段提取
class LogParser {
public:
    // 解析一行日志；不能识别头部时返回 false
    static bool parse(const QString& line, const QString& sourceFile, LogType type, LogEntry& out);
    // 各类型字段富化
    static void enrich(LogEntry& e);
    // 解析错误统计（用于"格式错误"检测，按源文件计数）
    static void resetParseStats();
    static QHash<QString, int> parseErrorStats();
};
