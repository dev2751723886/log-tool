#pragma once
#include <QString>
#include <QStringList>

// gzip / 纯文本统一读取（基于 zlib，静态链接）
class GzipReader {
public:
    static bool isGzip(const QString& path);
    // 读取全部行（自动判断 gzip / 纯文本），UTF-8 解码
    static bool readAllLines(const QString& path, QStringList& out);
};
