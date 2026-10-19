#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include "core/timeline.h"

// 运动轨迹：从 motion_control 的 location.pos_x/pos_y 绘制二维路径
class TrajectoryView : public QWidget {
    Q_OBJECT
public:
    explicit TrajectoryView(QWidget* parent = nullptr);
    void setData(const Timeline* tl);
    void setCursor(qint64 t);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    const Timeline* m_tl = nullptr;
    QVector<QPointF> m_points;
    QVector<qint64> m_times;
    qint64 m_cursor = -1;
};
