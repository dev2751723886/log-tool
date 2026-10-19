#pragma once
#include "core/logentry.h"
#include <QString>
#include <QStringList>
#include <QVector>

// 问题类型
enum class IssueType {
    Crash = 0,                // 程序崩溃
    NodeExecutionFailure,     // 节点执行失败
    SensorDataLoss,           // 传感器数据丢失
    ChassisControlTimeout,    // 底盘控制超时
    InterfaceError,           // 接口返回错误
    CrossValidationMismatch,  // 跨文件印证不一致
    // 仿真领域
    NumericalError,        // 数值异常（NaN/Inf）
    NonConvergence,        // 求解器不收敛
    DataInconsistency,     // 跨模块数据不一致
    PerformanceTimeout,    // 性能超时
    ConfigError,           // 配置/输入错误
    RegressionDeviation,   // 基准结果偏差
    TimestampAnomaly,      // 时间倒退/重复
    ParseError,            // 日志格式错误
    Count
};

enum class Severity { Critical = 0, Error, Warning, Info };

// 问题报告模型：类型 + 严重度 + 时间 + 前后文 + 候选原因 + 解决建议
struct Anomaly {
    IssueType type = IssueType::NodeExecutionFailure;
    Severity severity = Severity::Warning;
    qint64 timeMs = 0;
    QString timeStr;
    QString title;             // 一句话标题
    QString description;       // 发生经过（含前后文叙述）
    QStringList causes;        // 候选原因
    QStringList solutions;     // 解决建议
    QStringList relatedFiles;  // 涉及文件
    QVector<LogEntry> context; // 异常前后原始日志（用于追踪操作/状态变化/指令/结果）
    int count = 1;             // 同类聚合计数
    // 错误定位（结构化）
    QString errorType;         // 错误类型 / 错误码
    QString file;              // 涉及文件 / 函数
    int line = 0;              // 行号
};

inline QString issueTypeName(IssueType t) {
    switch (t) {
    case IssueType::Crash: return "程序崩溃";
    case IssueType::NodeExecutionFailure: return "节点执行失败";
    case IssueType::SensorDataLoss: return "传感器数据丢失";
    case IssueType::ChassisControlTimeout: return "底盘控制超时";
    case IssueType::InterfaceError: return "接口返回错误";
    case IssueType::CrossValidationMismatch: return "跨文件印证不一致";
    case IssueType::NumericalError: return "数值异常";
    case IssueType::NonConvergence: return "求解器不收敛";
    case IssueType::DataInconsistency: return "跨模块数据不一致";
    case IssueType::PerformanceTimeout: return "性能超时";
    case IssueType::ConfigError: return "配置/输入错误";
    case IssueType::RegressionDeviation: return "基准结果偏差";
    case IssueType::TimestampAnomaly: return "时间倒退/重复";
    case IssueType::ParseError: return "日志格式错误";
    default: return "未知";
    }
}

inline QString severityName(Severity s) {
    switch (s) {
    case Severity::Critical: return "严重";
    case Severity::Error: return "错误";
    case Severity::Warning: return "警告";
    case Severity::Info: return "提示";
    }
    return "";
}

// 反查（用于加载报告 / 对比）
inline IssueType issueTypeFromName(const QString& n) {
    if (n == "程序崩溃") return IssueType::Crash;
    if (n == "节点执行失败") return IssueType::NodeExecutionFailure;
    if (n == "传感器数据丢失") return IssueType::SensorDataLoss;
    if (n == "底盘控制超时") return IssueType::ChassisControlTimeout;
    if (n == "接口返回错误") return IssueType::InterfaceError;
    if (n == "跨文件印证不一致") return IssueType::CrossValidationMismatch;
    if (n == "数值异常") return IssueType::NumericalError;
    if (n == "求解器不收敛") return IssueType::NonConvergence;
    if (n == "跨模块数据不一致") return IssueType::DataInconsistency;
    if (n == "性能超时") return IssueType::PerformanceTimeout;
    if (n == "配置/输入错误") return IssueType::ConfigError;
    if (n == "基准结果偏差") return IssueType::RegressionDeviation;
    if (n == "时间倒退/重复") return IssueType::TimestampAnomaly;
    if (n == "日志格式错误") return IssueType::ParseError;
    return IssueType::NodeExecutionFailure;
}
inline Severity severityFromName(const QString& n) {
    if (n == "严重") return Severity::Critical;
    if (n == "错误") return Severity::Error;
    if (n == "警告") return Severity::Warning;
    if (n == "提示") return Severity::Info;
    return Severity::Info;
}
