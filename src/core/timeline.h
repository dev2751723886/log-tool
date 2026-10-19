#pragma once
#include "core/logentry.h"
#include <QVector>
#include <QHash>

// 统一时间轴：多文件事件合并 + 按时间排序 + 索引
class Timeline {
public:
    void clear();
    void reserve(int n);
    void append(LogEntry&& e);
    void buildIndex();   // 排序 + 建立类型索引

    int count() const { return m_all.size(); }
    const QVector<LogEntry>& entries() const { return m_all; }
    qint64 minTime() const { return m_all.isEmpty() ? 0 : m_all.first().epochMs; }
    qint64 maxTime() const { return m_all.isEmpty() ? 0 : m_all.last().epochMs; }

    // 时间窗口 [t0, t1] 内的条目下标
    QVector<int> range(qint64 t0, qint64 t1) const;
    QVector<int> byType(LogType t) const;

private:
    int lowerBound(qint64 t) const;
    QVector<LogEntry> m_all;
    QHash<int, QVector<int>> m_byType;
};
