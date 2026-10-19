#pragma once
#include <QMainWindow>
#include <QVector>
#include <QStringList>
#include "core/timeline.h"
#include "analyze/anomaly.h"
#include "analyze/runreport.h"

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QTextBrowser;
class QLabel;
class QComboBox;
class QPushButton;
class QSlider;
class QTimer;
class TimelineView;
class TrajectoryView;
class SensorCurveView;
class LogTableView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onBrowse();
    void onBrowseFiles();
    void onAnalyze();
    void onSaveReport();
    void onIssueSelected();
    void onCursorChanged(qint64 t);
    void onCurveFieldChanged(int idx);
    void onPlayToggle();
    void onTick();
    void onSliderMoved(int v);
    void onCompare();
    void onExportHtml();

private:
    void populateIssues();
    void showIssue(int idx);
    void setCursorAll(qint64 t);
    void populateCurveFields();
    void renderReport();

    QLineEdit* m_pathEdit = nullptr;
    QString m_dir;
    QStringList m_files;
    QTreeWidget* m_issueTree = nullptr;
    QTextBrowser* m_detail = nullptr;
    QTextBrowser* m_report = nullptr;
    QComboBox* m_curveCombo = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_playBtn = nullptr;
    QComboBox* m_speedCombo = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_timeLabel = nullptr;
    QTimer* m_timer = nullptr;
    TimelineView* m_timeline = nullptr;
    TrajectoryView* m_trajectory = nullptr;
    SensorCurveView* m_curve = nullptr;
    LogTableView* m_logTable = nullptr;

    Timeline m_timelineData;
    QVector<Anomaly> m_anomalies;
    QVector<RunSummary> m_runs;
    QVector<ConstraintCheck> m_constraints;
    bool m_playing = false;
    qint64 m_cursor = 0;
    qint64 m_tMin = 0, m_tMax = 0;
};
