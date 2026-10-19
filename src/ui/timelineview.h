#pragma once
#include <QWidget>
#include <QVector>
#include "core/timeline.h"
#include "analyze/anomaly.h"

// 统一时间轴：按像素分桶绘制所有日志事件的严重度分布 + 异常标记 + 可拖动游标
class TimelineView : public QWidget {
    Q_OBJECT
public:
    explicit TimelineView(QWidget* parent = nullptr);
    void setData(const Timeline* tl, const QVector<Anomaly>* anomalies);
    void setCursor(qint64 t);
    qint64 cursor() const { return m_cursor; }

signals:
    void cursorTimeChanged(qint64 t);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    qint64 timeAt(int x) const;
    int xAt(qint64 t) const;
    const Timeline* m_tl = nullptr;
    const QVector<Anomaly>* m_anom = nullptr;
    qint64 m_cursor = -1;
};
