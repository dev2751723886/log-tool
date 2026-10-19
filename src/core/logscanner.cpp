#include "core/logscanner.h"
#include <QDirIterator>
#include <QFileInfo>

QVector<LogFile> LogScanner::scan(const QString& rootDir) {
    QVector<LogFile> result;
    QDirIterator it(rootDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        LogType t = detectLogType(fi.fileName());
        if (t == LogType::Unknown) continue;
        if (fi.size() == 0) continue;  // 跳过空文件（如 obstacle_view）
        LogFile lf;
        lf.path = fi.absoluteFilePath();
        lf.name = fi.fileName();
        lf.type = t;
        lf.size = fi.size();
        result.append(lf);
    }
    return result;
}

QVector<LogFile> LogScanner::scanFiles(const QStringList& files) {
    QVector<LogFile> result;
    for (const QString& p : files) {
        QFileInfo fi(p);
        if (!fi.exists() || !fi.isFile() || fi.size() == 0) continue;
        LogFile lf;
        lf.path = fi.absoluteFilePath();
        lf.name = fi.fileName();
        lf.type = detectLogType(fi.fileName());
        lf.size = fi.size();
        result.append(lf);
    }
    return result;
}
