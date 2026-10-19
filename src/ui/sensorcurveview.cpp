#include "ui/sensorcurveview.h"
#include "ui/theme.h"
#include <QPainter>
#include <QPolygonF>
#include <QDateTime>

SensorCurveView::SensorCurveView(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
}

void SensorCurveView::setData(const Timeline* tl, const QString& field) {
    m_tl = tl;
    m_field = field;
    m_times.clear();
    m_vals.clear();
    if (tl && !field.isEmpty()) {
        for (const LogEntry& e : tl->entries()) {
            if (!e.has(field)) continue;
            m_times.append(e.epochMs);
            m_vals.append(e.num(field));
        }
    }
    update();
}

void SensorCurveView::setCursor(qint64 t) { m_cursor = t; update(); }

void SensorCurveView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Theme::background());
    if (m_times.size() < 2) {
        p.setPen(Theme::text());
        p.drawText(rect(), Qt::AlignCenter, "该字段无足够数据");
        return;
    }
    const int M = 30, bottomPad = 24;
    qint64 tmin = m_times.first(), tmax = m_times.last();
    if (tmax <= tmin) tmax = tmin + 1;
    double vmin = 1e18, vmax = -1e18;
    for (double v : m_vals) { vmin = qMin(vmin, v); vmax = qMax(vmax, v); }
    if (vmax <= vmin) vmax = vmin + 1;

    double sx = (width() - 2 * M) / (double)(tmax - tmin);
    double sy = (height() - M - bottomPad) / (vmax - vmin);
    auto map = [&](qint64 t, double v) {
        return QPointF(M + (t - tmin) * sx, height() - bottomPad - (v - vmin) * sy);
    };

    // 网格 + 轴
    p.setPen(Theme::grid());
    p.drawLine(M, M, M, height() - bottomPad);
    p.drawLine(M, height() - bottomPad, width() - M, height() - bottomPad);
    p.setPen(Theme::text());
    p.drawText(QRect(0, M - 16, width(), 14), Qt::AlignLeft | Qt::AlignVCenter, m_field);
    p.drawText(QRect(M, height() - bottomPad + 4, width() - M, 14), Qt::AlignRight,
               QString("min %1  max %2").arg(vmin).arg(vmax));

    // 曲线
    QPolygonF poly;
    for (int i = 0; i < m_times.size(); ++i) poly << map(m_times[i], m_vals[i]);
    p.setPen(QPen(QColor("#10B981"), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(poly);

    // 游标
    if (m_cursor >= tmin && m_cursor <= tmax) {
        int x = (int)(M + (m_cursor - tmin) * sx);
        p.setPen(QPen(QColor("#FFFFFF"), 1, Qt::DashLine));
        p.drawLine(x, M, x, height() - bottomPad);
    }
}
