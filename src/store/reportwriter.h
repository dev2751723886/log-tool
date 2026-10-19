#pragma once
#include "analyze/anomaly.h"
#include "analyze/runreport.h"
#include <QVector>
#include <QString>

// 分析结果持久化（阶段5：保存结果 / 对比修复前后）
class ReportWriter {
public:
    static QString toJson(const QVector<Anomaly>& anomalies);
    static bool save(const QVector<Anomaly>& anomalies, const QString& path);
    static bool load(const QString& path, QVector<Anomaly>& out);
    static QString summary(const QVector<Anomaly>& anomalies);
    static QString toHtml(const QVector<Anomaly>& anomalies,
                          const QVector<RunSummary>& runs,
                          const QVector<ConstraintCheck>& constraints);
    static bool saveHtml(const QString& path,
                         const QVector<Anomaly>& anomalies,
                         const QVector<RunSummary>& runs,
                         const QVector<ConstraintCheck>& constraints);

    // 修复前后对比结果
    struct CompareResult {
        QVector<Anomaly> resolved;    // 修复前有、修复后无
        QVector<Anomaly> added;       // 修复后新增
        QVector<Anomaly> persistent;  // 持续存在
    };
    static CompareResult compare(const QVector<Anomaly>& before, const QVector<Anomaly>& after);
};
