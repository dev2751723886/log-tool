#pragma once
#include <QString>
#include <QVariantMap>
#include <QVector>

// 日志类型（对应日志目录分类）
enum class LogType {
    Unknown = 0,
    BtStatus,        // 行为树状态
    MotionControl,   // 底盘 / MCU 反馈
    NavDefault,      // 导航主流程
    PlannerControl,  // 底盘规划控制
    Obstacle,        // 避障服务
    OaDetect,        // 避障检测结果
    GenericApp,      // 通用应用日志（.log / .txt）
    Simulation,      // 仿真日志（[time] [LEVEL] [module] key=value...）
    Count
};

// 统一日志条目模型：把不同节点/格式归一化到同一结构
struct LogEntry {
    qint64 epochMs = 0;        // 统一时间轴（epoch 毫秒，本地时区解释）
    QString timeStr;           // 原始时间串 [yyyy-MM-dd HH:mm:ss.mmm]
    LogType type = LogType::Unknown;
    QString node;              // 节点名
    QString level;             // info / warning / error / fatal / debug
    QString fileLine;          // 源文件:行号
    QString message;           // 消息体（超长会截断）
    QString sourceFile;        // 来源文件路径
    int lineNumber = 0;        // 在源文件中的行号（用于时间倒退/重复检测）
    QVariantMap fields;        // 结构化字段（bt 节点状态 / motion 数值 / 仿真 key=value 等）

    double num(const QString& k) const {
        bool ok = false;
        double v = fields.value(k).toDouble(&ok);
        return ok ? v : 0.0;
    }
    bool has(const QString& k) const { return fields.contains(k); }
    QString str(const QString& k) const { return fields.value(k).toString(); }
};

inline QString logTypeName(LogType t) {
    switch (t) {
    case LogType::BtStatus: return "bt_status";
    case LogType::MotionControl: return "motion_control_chip";
    case LogType::NavDefault: return "nav_default";
    case LogType::PlannerControl: return "planner_control";
    case LogType::Obstacle: return "obstacle";
    case LogType::OaDetect: return "oa_detect";
    case LogType::GenericApp: return "app_log";
    case LogType::Simulation: return "simulation";
    default: return "unknown";
    }
}

inline QString logTypeLabel(LogType t) {
    switch (t) {
    case LogType::BtStatus: return "行为树";
    case LogType::MotionControl: return "底盘反馈";
    case LogType::NavDefault: return "导航";
    case LogType::PlannerControl: return "规划控制";
    case LogType::Obstacle: return "避障";
    case LogType::OaDetect: return "避障检测";
    case LogType::GenericApp: return "应用日志";
    case LogType::Simulation: return "仿真";
    default: return "未知";
    }
}

inline LogType detectLogType(const QString& fileName) {
    const QString f = fileName.toLower();
    if (f.startsWith("bt_status")) return LogType::BtStatus;
    if (f.startsWith("motion_control")) return LogType::MotionControl;
    if (f.startsWith("nav_default")) return LogType::NavDefault;
    if (f.startsWith("planner_control")) return LogType::PlannerControl;
    if (f.startsWith("obstacle")) return LogType::Obstacle;
    if (f.startsWith("oa_detect")) return LogType::OaDetect;
    if (f.endsWith(".log") || f.endsWith(".txt")) return LogType::GenericApp;
    return LogType::Unknown;
}
