#include "ui/mainwindow.h"
#include "ui/theme.h"
#include "ui/timelineview.h"
#include "ui/trajectoryview.h"
#include "ui/sensorcurveview.h"
#include "ui/logtableview.h"
#include "ui/comparedialog.h"
#include "core/logscanner.h"
#include "core/gzipreader.h"
#include "core/logparser.h"
#include "analyze/ruleengine.h"
#include "store/reportwriter.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QTimer>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QElapsedTimer>
#include <QApplication>
#include <QSignalBlocker>
#include <QDateTime>
#include <algorithm>
#include <utility>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("日志检查系统");
    resize(1280, 840);

    auto* central = new QWidget(this);
    auto* rootLay = new QVBoxLayout(central);

    // 顶部：路径选择 + 操作
    auto* topBar = new QHBoxLayout();
    m_pathEdit = new QLineEdit(central);
    m_pathEdit->setPlaceholderText("选择日志目录（递归扫描）或日志文件");
    auto* browseDirBtn = new QPushButton("选择目录", central);
    auto* browseFileBtn = new QPushButton("选择文件", central);
    auto* analyzeBtn = new QPushButton("开始检测", central);
    auto* saveBtn = new QPushButton("保存报告", central);
    auto* htmlBtn = new QPushButton("导出HTML", central);
    auto* compareBtn = new QPushButton("对比报告", central);
    topBar->addWidget(new QLabel("日志路径:", central));
    topBar->addWidget(m_pathEdit, 1);
    topBar->addWidget(browseDirBtn);
    topBar->addWidget(browseFileBtn);
    topBar->addWidget(analyzeBtn);
    topBar->addWidget(saveBtn);
    topBar->addWidget(htmlBtn);
    topBar->addWidget(compareBtn);
    rootLay->addLayout(topBar);

    // 回放控制条
    auto* playBar = new QHBoxLayout();
    m_playBtn = new QPushButton("播放", central);
    m_playBtn->setCheckable(false);
    m_speedCombo = new QComboBox(central);
    const int mults[] = {1, 10, 100, 1000, 5000, 20000};
    for (int m : mults) m_speedCombo->addItem(QString("%1x").arg(m), m);
    m_speedCombo->setCurrentIndex(4);  // 默认 5000x
    m_slider = new QSlider(Qt::Horizontal, central);
    m_slider->setRange(0, 1000);
    m_timeLabel = new QLabel("--", central);
    m_timeLabel->setMinimumWidth(150);
    playBar->addWidget(m_playBtn);
    playBar->addWidget(m_speedCombo);
    playBar->addWidget(m_slider, 1);
    playBar->addWidget(m_timeLabel);
    rootLay->addLayout(playBar);

    // 中部：左问题列表 + 右详情/视图
    auto* splitter = new QSplitter(Qt::Horizontal, central);

    auto* leftWidget = new QWidget(splitter);
    auto* leftLay = new QVBoxLayout(leftWidget);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->addWidget(new QLabel("发现的问题", leftWidget));
    m_issueTree = new QTreeWidget(leftWidget);
    m_issueTree->setHeaderLabels(QStringList() << "时间" << "严重度" << "类型" << "标题");
    m_issueTree->setRootIsDecorated(false);
    m_issueTree->setAlternatingRowColors(true);
    leftLay->addWidget(m_issueTree);

    auto* tabs = new QTabWidget(splitter);
    m_detail = new QTextBrowser(tabs);
    tabs->addTab(m_detail, "问题详情");
    m_report = new QTextBrowser(tabs);
    tabs->addTab(m_report, "运行报告");
    m_timeline = new TimelineView(tabs);
    tabs->addTab(m_timeline, "时间轴");
    m_trajectory = new TrajectoryView(tabs);
    tabs->addTab(m_trajectory, "轨迹");
    auto* curveWidget = new QWidget(tabs);
    auto* curveLay = new QVBoxLayout(curveWidget);
    curveLay->setContentsMargins(0, 0, 0, 0);
    m_curveCombo = new QComboBox(curveWidget);
    m_curve = new SensorCurveView(curveWidget);
    curveLay->addWidget(m_curveCombo);
    curveLay->addWidget(m_curve);
    tabs->addTab(curveWidget, "波形图");
    m_logTable = new LogTableView(tabs);
    tabs->addTab(m_logTable, "原始日志");

    splitter->addWidget(leftWidget);
    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 560 << 720);
    rootLay->addWidget(splitter, 1);

    m_status = new QLabel("请选择日志目录后点击“开始检测”", central);
    rootLay->addWidget(m_status);

    setCentralWidget(central);

    m_timer = new QTimer(this);
    m_timer->setInterval(50);

    connect(browseDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowse);
    connect(browseFileBtn, &QPushButton::clicked, this, &MainWindow::onBrowseFiles);
    connect(analyzeBtn, &QPushButton::clicked, this, &MainWindow::onAnalyze);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveReport);
    connect(htmlBtn, &QPushButton::clicked, this, &MainWindow::onExportHtml);
    connect(compareBtn, &QPushButton::clicked, this, &MainWindow::onCompare);
    connect(m_issueTree, &QTreeWidget::currentItemChanged, this, &MainWindow::onIssueSelected);
    connect(m_timeline, &TimelineView::cursorTimeChanged, this, &MainWindow::onCursorChanged);
    connect(m_curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCurveFieldChanged);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayToggle);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTick);
    connect(m_slider, &QSlider::valueChanged, this, &MainWindow::onSliderMoved);
}

void MainWindow::onBrowse() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择日志目录（递归扫描子目录）", m_dir);
    if (!dir.isEmpty()) {
        m_dir = dir;
        m_files.clear();
        m_pathEdit->setText(dir);
    }
}

void MainWindow::onBrowseFiles() {
    QStringList files = QFileDialog::getOpenFileNames(this, "选择日志文件（可多选）", "",
        "所有文件 (*);;日志文件 (*.gz *.log *.txt)");
    if (!files.isEmpty()) {
        m_files = files;
        m_dir.clear();
        m_pathEdit->setText(QString("已选择 %1 个文件").arg(files.size()));
    }
}

void MainWindow::onAnalyze() {
    if (m_dir.isEmpty() && m_files.isEmpty()) {
        m_status->setText("请先选择日志目录或文件");
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_status->setText("正在扫描日志...");
    QApplication::processEvents();

    QElapsedTimer timer;
    timer.start();
    QVector<LogFile> files;
    if (!m_dir.isEmpty()) files = LogScanner::scan(m_dir);
    if (!m_files.isEmpty()) files += LogScanner::scanFiles(m_files);
    if (files.isEmpty()) {
        QApplication::restoreOverrideCursor();
        m_status->setText("未找到可识别的日志文件");
        return;
    }
    m_timelineData.clear();
    int parsed = 0;
    LogParser::resetParseStats();
    for (const LogFile& lf : files) {
        QStringList lines;
        if (!GzipReader::readAllLines(lf.path, lines)) continue;
        for (int li = 0; li < lines.size(); ++li) {
            LogEntry e;
            if (LogParser::parse(lines[li], lf.path, lf.type, e)) {
                e.lineNumber = li + 1;
                m_timelineData.append(std::move(e));
                parsed++;
            }
        }
    }
    m_timelineData.buildIndex();

    RuleEngine engine;
    m_anomalies = engine.analyze(m_timelineData);
    m_runs = engine.runs();
    m_constraints = engine.constraints();

    m_tMin = m_timelineData.minTime();
    m_tMax = m_timelineData.maxTime();
    m_cursor = m_tMin;

    populateIssues();
    renderReport();
    m_timeline->setData(&m_timelineData, &m_anomalies);
    m_trajectory->setData(&m_timelineData);
    m_logTable->setTimeline(&m_timelineData);
    populateCurveFields();
    m_curve->setData(&m_timelineData, m_curveCombo->currentData().toString());
    setCursorAll(m_cursor);

    QApplication::restoreOverrideCursor();
    m_status->setText(QString("完成：扫描 %1 个文件，解析 %2 条，发现 %3 个问题（%4 ms）")
                          .arg(files.size()).arg(parsed).arg(m_anomalies.size()).arg(timer.elapsed()));
}

// 数值字段 → 中文标签
static QString fieldLabel(const QString& key) {
    static const QHash<QString, QString> m = {
        {"residual", "残差"}, {"iter", "迭代次数"}, {"state_x", "状态量 x"},
        {"velocity", "速度"}, {"t", "时间 t"}, {"dt", "步长 dt"},
        {"temperature", "温度"}, {"pressure", "压力"}, {"exit_code", "退出码"},
        {"elapsed_ms", "耗时(ms)"}, {"max_iter", "最大迭代次数"}, {"tolerance", "容差"},
        {"baseline", "基准值"}, {"actual", "实际值"}, {"abs_diff", "绝对偏差"},
        {"rel_diff", "相对偏差"}, {"elements", "单元数"}, {"nodes", "节点数"},
        {"cells", "网格数"}, {"timeout_ms", "超时阈值(ms)"}, {"threshold_ms", "阈值(ms)"},
        {"attempt", "重试次数"}, {"max_attempts", "最大重试次数"}, {"msg_index", "消息序号"},
        {"position", "位置"}, {"pos_x", "位置 x"}, {"pos_y", "位置 y"}, {"pos_heading", "航向角"},
        {"step", "步号"}, {"step_id", "步 ID"}, {"value", "数值"},
        {"rpm", "电机转速"}, {"I", "电机电流"}, {"T", "电机温度"}, {"yaw", "航向"},
        {"roll", "横滚"}, {"pitch", "俯仰"}, {"vel_x", "速度 x"}, {"omg_yaw", "角速度"},
        {"motion_state", "运动状态"}, {"location_state", "定位状态"}, {"mot_state", "电机状态"},
        {"position_confidence", "位置置信度"}, {"angle_confidence", "角度置信度"},
        {"estop_flag", "急停标志"}, {"control_type", "控制类型"},
        {"chassis.imu.roll", "IMU 横滚"}, {"chassis.imu.pitch", "IMU 俯仰"},
        {"chassis.imu.yaw", "IMU 航向"}, {"chassis.location.pos_x", "定位 x"},
        {"chassis.location.pos_y", "定位 y"}, {"chassis.location.odom.vel_x", "里程计速度"},
        {"chassis.chassis_model.drive_wheel.motor_driver.rpm", "电机转速 rpm"},
        {"chassis.chassis_model.drive_wheel.motor_driver.I", "电机电流 A"},
        {"axis.tray_lift.position", "托盘升降位置"},
    };
    QString full = m.value(key);
    if (!full.isEmpty()) return full;
    int dot = key.lastIndexOf('.');
    QString leaf = (dot >= 0) ? key.mid(dot + 1) : key;
    return m.value(leaf, leaf);
}

void MainWindow::populateCurveFields() {
    // 自动收集所有数值型字段（出现次数越多越靠前），用于波形/曲线选择
    QHash<QString, int> cnt;
    for (const LogEntry& e : m_timelineData.entries()) {
        for (auto it = e.fields.begin(); it != e.fields.end(); ++it) {
            bool ok = false;
            it.value().toDouble(&ok);
            if (ok && !it.key().isEmpty()) cnt[it.key()]++;
        }
    }
    QVector<QPair<QString, int>> list;
    for (auto it = cnt.begin(); it != cnt.end(); ++it) list.append(qMakePair(it.key(), it.value()));
    std::sort(list.begin(), list.end(), [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
        return a.second > b.second;
    });
    m_curveCombo->clear();
    for (const auto& f : list)
        m_curveCombo->addItem(QString("%1 (%2)").arg(fieldLabel(f.first)).arg(f.second), f.first);
}

void MainWindow::setCursorAll(qint64 t) {
    m_cursor = t;
    m_timeline->setCursor(t);
    m_trajectory->setCursor(t);
    m_curve->setCursor(t);
    QSignalBlocker b(m_slider);
    if (m_tMax > m_tMin) {
        int v = (int)((t - m_tMin) * 1000 / (m_tMax - m_tMin));
        m_slider->setValue(qBound(0, v, 1000));
    }
    m_timeLabel->setText(QDateTime::fromMSecsSinceEpoch(t).toString("yyyy-MM-dd HH:mm:ss"));
}

void MainWindow::populateIssues() {
    m_issueTree->clear();
    for (int i = 0; i < m_anomalies.size(); ++i) {
        const Anomaly& a = m_anomalies[i];
        auto* item = new QTreeWidgetItem();
        item->setText(0, a.timeStr);
        item->setText(1, severityName(a.severity));
        item->setText(2, issueTypeName(a.type));
        item->setText(3, a.title);
        item->setForeground(1, Theme::severityColor(a.severity));
        item->setData(0, Qt::UserRole, i);
        m_issueTree->addTopLevelItem(item);
    }
}

void MainWindow::onIssueSelected() {
    auto* item = m_issueTree->currentItem();
    if (!item) return;
    int idx = item->data(0, Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_anomalies.size()) return;
    showIssue(idx);
    setCursorAll(m_anomalies[idx].timeMs);
}

void MainWindow::showIssue(int idx) {
    const Anomaly& a = m_anomalies[idx];
    QString html;
    html += QString("<h2 style='color:%1'>[%2] %3</h2>")
                .arg(Theme::severityColor(a.severity).name(), severityName(a.severity), a.title);
    html += QString("<p><b>时间：</b>%1 &nbsp; <b>类型：</b>%2 &nbsp; <b>出现：</b>%3 次</p>")
                .arg(a.timeStr, issueTypeName(a.type)).arg(a.count);
    html += "<h3>问题经过</h3>";
    html += QString("<p>%1</p>").arg(a.description.toHtmlEscaped());
    html += "<h3>候选原因</h3><ul>";
    for (const QString& c : a.causes) html += QString("<li>%1</li>").arg(c.toHtmlEscaped());
    html += "</ul>";
    html += "<h3>解决建议</h3><ul>";
    for (const QString& s : a.solutions) html += QString("<li>%1</li>").arg(s.toHtmlEscaped());
    html += "</ul>";
    if (!a.relatedFiles.isEmpty()) {
        html += "<h3>涉及文件</h3><ul>";
        for (const QString& f : a.relatedFiles) html += QString("<li>%1</li>").arg(f.toHtmlEscaped());
        html += "</ul>";
    }
    if (!a.context.isEmpty()) {
        html += "<h3>异常前后日志（追踪操作 / 状态变化 / 指令 / 结果）</h3><pre style='font-size:11px'>";
        for (const LogEntry& e : a.context) {
            QString msg = e.message;
            if (msg.size() > 140) msg = msg.left(140) + "...";
            msg = msg.toHtmlEscaped();
            QString line = e.timeStr + " [" + e.node + "] [" + e.level + "] " + msg;

            QString lv = e.level.toLower();
            bool isFail = (e.str("bt_cur") == "FAILURE");
            if (lv == "fatal" || lv == "error" || isFail) {
                html += "<span style='color:#E5484D;font-weight:bold'>" + line + "</span>\n";
            } else if (lv == "warning" || lv == "warn") {
                html += "<span style='color:#F59E0B'>" + line + "</span>\n";
            } else {
                html += line + "\n";
            }
        }
        html += "</pre>";
    }
    m_detail->setHtml(html);
}

void MainWindow::onCursorChanged(qint64 t) {
    setCursorAll(t);
}

void MainWindow::onCurveFieldChanged(int) {
    m_curve->setData(&m_timelineData, m_curveCombo->currentData().toString());
    m_curve->setCursor(m_cursor);
}

void MainWindow::onPlayToggle() {
    if (m_tMax <= m_tMin) { m_status->setText("请先完成检测再回放"); return; }
    m_playing = !m_playing;
    if (m_playing) {
        if (m_cursor >= m_tMax) m_cursor = m_tMin;
        m_timer->start();
        m_playBtn->setText("暂停");
    } else {
        m_timer->stop();
        m_playBtn->setText("播放");
    }
}

void MainWindow::onTick() {
    if (!m_playing || m_tMax <= m_tMin) return;
    int mult = m_speedCombo->currentData().toInt();
    m_cursor += (qint64)mult * 50;
    if (m_cursor >= m_tMax) {
        m_cursor = m_tMax;
        m_playing = false;
        m_timer->stop();
        m_playBtn->setText("播放");
    }
    setCursorAll(m_cursor);
}

void MainWindow::onSliderMoved(int v) {
    if (m_tMax <= m_tMin) return;
    qint64 t = m_tMin + (qint64)((m_tMax - m_tMin) * (v / 1000.0));
    setCursorAll(t);
}

void MainWindow::onCompare() {
    CompareDialog dlg(this);
    dlg.exec();
}

void MainWindow::renderReport() {
    m_report->setHtml(ReportWriter::toHtml(m_anomalies, m_runs, m_constraints));
}

void MainWindow::onExportHtml() {
    if (m_anomalies.isEmpty()) { m_status->setText("暂无可导出的分析结果"); return; }
    QString path = QFileDialog::getSaveFileName(this, "导出 HTML 报告", "运行报告.html", "HTML (*.html)");
    if (path.isEmpty()) return;
    if (ReportWriter::saveHtml(path, m_anomalies, m_runs, m_constraints))
        m_status->setText("报告已导出：" + path);
    else
        m_status->setText("导出失败：" + path);
}

void MainWindow::onSaveReport() {
    if (m_anomalies.isEmpty()) { m_status->setText("暂无可保存的分析结果"); return; }
    QString path = QFileDialog::getSaveFileName(this, "保存分析报告", "分析报告.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    if (ReportWriter::save(m_anomalies, path))
        m_status->setText("报告已保存：" + path);
    else
        m_status->setText("保存失败：" + path);
}
