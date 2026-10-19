#include "core/timeline.h"
#include <algorithm>
#include <utility>

void Timeline::clear() { m_all.clear(); m_byType.clear(); }
void Timeline::reserve(int n) { m_all.reserve(n); }

void Timeline::append(LogEntry&& e) { m_all.append(std::move(e)); }

void Timeline::buildIndex() {
    std::stable_sort(m_all.begin(), m_all.end(),
                     [](const LogEntry& a, const LogEntry& b) { return a.epochMs < b.epochMs; });
    m_byType.clear();
    for (int i = 0; i < m_all.size(); ++i) {
        m_byType[(int)m_all[i].type].append(i);
    }
}

int Timeline::lowerBound(qint64 t) const {
    int lo = 0, hi = m_all.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m_all[mid].epochMs < t) lo = mid + 1; else hi = mid;
    }
    return lo;
}

QVector<int> Timeline::range(qint64 t0, qint64 t1) const {
    QVector<int> out;
    int i = lowerBound(t0);
    for (; i < m_all.size(); ++i) {
        if (m_all[i].epochMs > t1) break;
        out.append(i);
    }
    return out;
}

QVector<int> Timeline::byType(LogType t) const {
    return m_byType.value((int)t);
}
