#pragma once
#include <QWidget>
#include <QVector>
#include "core/timeline.h"

// 传感器曲线：绘制某个数值字段随时间的曲线（含游标）
class SensorCurveView : public QWidget {
    Q_OBJECT
public:
    explicit SensorCurveView(QWidget* parent = nullptr);
    void setData(const Timeline* tl, const QString& field);
    void setCursor(qint64 t);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    const Timeline* m_tl = nullptr;
    QString m_field;
    QVector<qint64> m_times;
    QVector<double> m_vals;
    qint64 m_cursor = -1;
};
