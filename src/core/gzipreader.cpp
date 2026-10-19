#include "core/gzipreader.h"
#include <QFile>
#include <zlib.h>

bool GzipReader::isGzip(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray head = f.read(2);
    f.close();
    return head.size() >= 2 && (quint8)head[0] == 0x1f && (quint8)head[1] == 0x8b;
}

bool GzipReader::readAllLines(const QString& path, QStringList& out) {
    if (isGzip(path)) {
        gzFile gz = gzopen(path.toLocal8Bit().constData(), "rb");
        if (!gz) return false;
        char buf[1 << 16];
        QByteArray pending;
        int n = 0;
        while ((n = gzread(gz, buf, (int)sizeof(buf))) > 0) {
            pending.append(buf, n);
            int idx = 0;
            while ((idx = pending.indexOf('\n')) >= 0) {
                QByteArray line = pending.left(idx);
                if (line.endsWith('\r')) line.chop(1);
                out.append(QString::fromUtf8(line));
                pending.remove(0, idx + 1);
            }
        }
        if (!pending.isEmpty()) out.append(QString::fromUtf8(pending));
        gzclose(gz);
        return true;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    while (!f.atEnd()) {
        QByteArray line = f.readLine();
        if (line.endsWith('\n')) line.chop(1);
        if (line.endsWith('\r')) line.chop(1);
        out.append(QString::fromUtf8(line));
    }
    f.close();
    return true;
}
