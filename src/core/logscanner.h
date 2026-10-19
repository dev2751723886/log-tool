#pragma once
#include "core/logentry.h"
#include <QString>
#include <QStringList>
#include <QVector>

struct LogFile {
    QString path;              // 绝对路径
    QString name;              // 文件名
    LogType type = LogType::Unknown;
    qint64 size = 0;
};

// 自动扫描目录，识别所有支持的日志文件（递归）
class LogScanner {
public:
    static QVector<LogFile> scan(const QString& rootDir);
    // 直接扫描用户选择的文件列表
    static QVector<LogFile> scanFiles(const QStringList& files);
};
