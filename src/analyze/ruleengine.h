#pragma once
#include "analyze/anomaly.h"
#include "analyze/runreport.h"
#include "core/timeline.h"
#include <QVector>

// 规则引擎：对统一时间轴做检测 + 跨文件印证 + 仿真运行汇总/约束
class RuleEngine {
public:
    QVector<Anomaly> analyze(const Timeline& tl);
    const QVector<RunSummary>& runs() const { return m_runs; }
    const QVector<ConstraintCheck>& constraints() const { return m_constraints; }

private:
    void detectCrashes(const Timeline& tl, QVector<Anomaly>& out);
    void detectNodeFailures(const Timeline& tl, QVector<Anomaly>& out);
    void detectSensorLoss(const Timeline& tl, QVector<Anomaly>& out);
    void detectChassisTimeout(const Timeline& tl, QVector<Anomaly>& out);
    void detectInterfaceErrors(const Timeline& tl, QVector<Anomaly>& out);
    void crossValidate(const Timeline& tl, QVector<Anomaly>& out);
    void detectSimulationIssues(const Timeline& tl, QVector<Anomaly>& out);
    void detectTimestampAnomaly(const Timeline& tl, QVector<Anomaly>& out);
    void computeRunSummaries(const Timeline& tl);
    void computeConstraints(const Timeline& tl);

    QVector<LogEntry> contextWindow(const Timeline& tl, qint64 t,
                                    qint64 beforeMs, qint64 afterMs, int max) const;

    QVector<RunSummary> m_runs;
    QVector<ConstraintCheck> m_constraints;
};
