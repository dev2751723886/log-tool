#include "ui/trajectoryview.h"
#include "ui/theme.h"
#include <QPainter>
#include <QPolygonF>

TrajectoryView::TrajectoryView(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
}

void TrajectoryView::setData(const Timeline* tl) {
    m_tl = tl;
    m_points.clear();
    m_times.clear();
    if (tl) {
        for (const LogEntry& e : tl->entries()) {
            if (e.type != LogType::MotionControl) continue;
            if (!e.has("chassis.location.pos_x") || !e.has("chassis.location.pos_y")) continue;
            m_points.append(QPointF(e.num("chassis.location.pos_x"), e.num("chassis.location.pos_y")));
            m_times.append(e.epochMs);
        }
    }
    update();
}

void TrajectoryView::setCursor(qint64 t) { m_cursor = t; update(); }

void TrajectoryView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Theme::background());
    if (m_points.size() < 2) {
        p.setPen(Theme::text());
        p.drawText(rect(), Qt::AlignCenter, "轨迹数据不足（需 motion_control 定位数据）");
        return;
    }
    double minx = 1e18, maxx = -1e18, miny = 1e18, maxy = -1e18;
    for (const QPointF& q : m_points) {
        minx = qMin(minx, q.x()); maxx = qMax(maxx, q.x());
        miny = qMin(miny, q.y()); maxy = qMax(maxy, q.y());
    }
    if (maxx <= minx) maxx = minx + 1;
    if (maxy <= miny) maxy = miny + 1;
    const int M = 30;
    double sx = (width() - 2 * M) / (maxx - minx);
    double sy = (height() - 2 * M) / (maxy - miny);
    auto map = [&](const QPointF& q) {
        return QPointF(M + (q.x() - minx) * sx, height() - M - (q.y() - miny) * sy);
    };

    QPolygonF poly;
    for (const QPointF& q : m_points) poly << map(q);
    p.setPen(QPen(QColor("#10B981"), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(poly);

    // 起终点
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#10B981"));
    p.drawEllipse(map(m_points.first()), 4, 4);
    p.setBrush(QColor("#3B82F6"));
    p.drawEllipse(map(m_points.last()), 4, 4);

    // 游标点
    if (m_cursor > 0 && !m_times.isEmpty()) {
        int idx = 0;
        for (int i = 0; i < m_times.size(); ++i) {
            if (m_times[i] <= m_cursor) idx = i; else break;
        }
        p.setBrush(QColor("#FFFFFF"));
        p.drawEllipse(map(m_points[idx]), 5, 5);
    }
}
