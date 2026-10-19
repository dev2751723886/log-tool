#pragma once
#include <QString>
#include <QVector>

// 仿真运行汇总（PASS/FAIL 判定）
struct RunSummary {
    QString runId;
    QString model;
    qint64 startMs = 0, endMs = 0;
    QString verdict = "PASS";   // PASS / FAIL / WARNING
    QString failReason;         // FAIL 时的原因
    int errorCount = 0;
    int warnCount = 0;
};

// 约束/断言检查项
struct ConstraintCheck {
    QString runId;
    QString name;       // 约束名
    QString expected;   // 期望
    QString actual;     // 实际
    bool passed;        // 是否满足
    QString evidence;   // 证据（原始日志）
};
