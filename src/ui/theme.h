#pragma once
#include <QColor>
#include <QString>
#include "analyze/anomaly.h"
#include "core/logentry.h"

// 统一配色（暗色主题，保证各视图一致）
namespace Theme {
    inline QColor severityColor(Severity s) {
        switch (s) {
        case Severity::Critical: return QColor("#E5484D");
        case Severity::Error: return QColor("#E5484D");
        case Severity::Warning: return QColor("#F59E0B");
        case Severity::Info: return QColor("#3B82F6");
        }
        return QColor("#8892A6");
    }
    inline QColor typeColor(LogType t) {
        switch (t) {
        case LogType::BtStatus: return QColor("#8B5CF6");
        case LogType::MotionControl: return QColor("#10B981");
        case LogType::NavDefault: return QColor("#3B82F6");
        case LogType::PlannerControl: return QColor("#F59E0B");
        case LogType::Obstacle: return QColor("#EC4899");
        case LogType::OaDetect: return QColor("#14B8A6");
        case LogType::GenericApp: return QColor("#A78BFA");
        case LogType::Simulation: return QColor("#22D3EE");
        default: return QColor("#8892A6");
        }
    }
    inline QColor levelColor(const QString& level) {
        QString l = level.toLower();
        if (l == "fatal") return QColor("#B42318");
        if (l == "error") return QColor("#E5484D");
        if (l == "warning" || l == "warn") return QColor("#F59E0B");
        if (l == "debug") return QColor("#8892A6");
        return QColor("#5B8DEF");
    }
    inline const QColor& background() { static QColor c("#14171f"); return c; }
    inline const QColor& grid() { static QColor c("#2a3040"); return c; }
    inline const QColor& text() { static QColor c("#c8d0e0"); return c; }
}
