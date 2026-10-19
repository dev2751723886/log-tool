#include "ui/timelineview.h"
#include "ui/theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>

TimelineView::TimelineView(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(170);
    setMouseTracking(true);
}

void TimelineView::setData(const Timeline* tl, const QVector<Anomaly>* anomalies) {
    m_tl = tl; m_anom = anomalies;
    update();
}

void TimelineView::setCursor(qint64 t) { m_cursor = t; update(); }

int TimelineView::xAt(qint64 t) const {
    if (!m_tl || m_tl->count() == 0) return 0;
    qint64 tmin = m_tl->minTime(), tmax = m_tl->maxTime();
    if (tmax <= tmin) tmax = tmin + 1;
    return (int)((t - tmin) / (double)(tmax - tmin) * width());
}

qint64 TimelineView::timeAt(int x) const {
    if (!m_tl || m_tl->count() == 0) return 0;
    qint64 tmin = m_tl->minTime(), tmax = m_tl->maxTime();
    if (tmax <= tmin) tmax = tmin + 1;
    int w = qMax(1, width());
    return tmin + (qint64)((tmax - tmin) * (x / (double)w));
}

void TimelineView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Theme::background());
    if (!m_tl || m_tl->count() == 0) {
        p.setPen(Theme::text());
        p.drawText(rect(), Qt::AlignCenter, "暂无数据");
        return;
    }
    const int W = width(), H = height();
    const int topBand = 26, bottomPad = 26;
    const int plotH = H - topBand - bottomPad;

    qint64 tmin = m_tl->minTime(), tmax = m_tl->maxTime();
    if (tmax <= tmin) tmax = tmin + 1;

    auto levelRank = [](const QString& l) {
        QString s = l.toLower();
        if (s == "fatal") return 5;
        if (s == "error") return 4;
        if (s == "warning" || s == "warn") return 3;
        if (s == "info") return 2;
        if (s == "debug") return 1;
        return 2;
    };

    // 像素分桶：每列取最严重级别
    QVector<int> worst(W, 0);
    for (const LogEntry& e : m_tl->entries()) {
        int x = xAt(e.epochMs);
        if (x < 0 || x >= W) continue;
        int r = levelRank(e.level);
        if (r > worst[x]) worst[x] = r;
    }
    for (int x = 0; x < W; ++x) {
        if (worst[x] == 0) continue;
        QColor c;
        switch (worst[x]) {
        case 5: c = QColor("#B42318"); break;
        case 4: c = QColor("#E5484D"); break;
        case 3: c = QColor("#F59E0B"); break;
        case 2: c = QColor("#5B8DEF"); break;
        default: c = QColor("#3a4150"); break;
        }
        p.setPen(c);
        p.drawLine(x, topBand, x, topBand + plotH);
    }

    // 异常标记（顶部三角）
    if (m_anom) {
        for (const Anomaly& a : *m_anom) {
            int x = xAt(a.timeMs);
            if (x < 0 || x >= W) continue;
            p.setPen(Qt::NoPen);
            p.setBrush(Theme::severityColor(a.severity));
            QPolygon tri;
            tri << QPoint(x, 2) << QPoint(x - 4, topBand - 2) << QPoint(x + 4, topBand - 2);
            p.drawPolygon(tri);
        }
    }

    // 时间轴
    int baseY = topBand + plotH;
    p.setPen(Theme::grid());
    p.drawLine(0, baseY, W, baseY);
    p.setPen(Theme::text());
    const int nTicks = 6;
    for (int i = 0; i <= nTicks; ++i) {
        int x = i * W / nTicks;
        qint64 t = tmin + (qint64)((tmax - tmin) * i / nTicks);
        p.drawLine(x, baseY, x, baseY + 4);
        p.drawText(QRect(x - 60, baseY + 6, 120, 14), Qt::AlignCenter,
                   QDateTime::fromMSecsSinceEpoch(t).toString("HH:mm:ss"));
    }

    // 游标
    if (m_cursor >= tmin && m_cursor <= tmax) {
        int x = xAt(m_cursor);
        p.setPen(QPen(QColor("#FFFFFF"), 1));
        p.drawLine(x, 0, x, baseY);
    }
}

void TimelineView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_cursor = timeAt(e->x());
        emit cursorTimeChanged(m_cursor);
        update();
    }
}

void TimelineView::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton) {
        m_cursor = timeAt(e->x());
        emit cursorTimeChanged(m_cursor);
        update();
    }
}
