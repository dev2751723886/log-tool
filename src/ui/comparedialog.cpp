#include "ui/comparedialog.h"
#include "ui/theme.h"
#include "store/reportwriter.h"
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

CompareDialog::CompareDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("修复前后对比");
    resize(820, 640);

    auto* form = new QFormLayout();
    m_before = new QLineEdit(this);
    m_after = new QLineEdit(this);
    auto* beforeRow = new QHBoxLayout();
    auto* afterRow = new QHBoxLayout();
    beforeRow->addWidget(m_before, 1);
    afterRow->addWidget(m_after, 1);
    auto* b1 = new QPushButton("浏览", this);
    auto* b2 = new QPushButton("浏览", this);
    beforeRow->addWidget(b1);
    afterRow->addWidget(b2);
    form->addRow("修复前报告:", beforeRow);
    form->addRow("修复后报告:", afterRow);

    auto* compareBtn = new QPushButton("开始对比", this);
    m_result = new QTextBrowser(this);

    auto* lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(compareBtn);
    lay->addWidget(m_result, 1);

    connect(b1, &QPushButton::clicked, this, &CompareDialog::browseBefore);
    connect(b2, &QPushButton::clicked, this, &CompareDialog::browseAfter);
    connect(compareBtn, &QPushButton::clicked, this, &CompareDialog::runCompare);
}

void CompareDialog::browseBefore() {
    QString p = QFileDialog::getOpenFileName(this, "选择修复前报告", "", "JSON (*.json)");
    if (!p.isEmpty()) m_before->setText(p);
}
void CompareDialog::browseAfter() {
    QString p = QFileDialog::getOpenFileName(this, "选择修复后报告", "", "JSON (*.json)");
    if (!p.isEmpty()) m_after->setText(p);
}

void CompareDialog::runCompare() {
    QVector<Anomaly> before, after;
    if (m_before->text().isEmpty() || m_after->text().isEmpty()) {
        m_result->setPlainText("请先选择修复前、修复后两份报告文件。");
        return;
    }
    if (!ReportWriter::load(m_before->text().trimmed(), before) ||
        !ReportWriter::load(m_after->text().trimmed(), after)) {
        m_result->setPlainText("报告加载失败，请检查文件路径与格式。");
        return;
    }
    ReportWriter::CompareResult r = ReportWriter::compare(before, after);

    QString html;
    html += QString("<h2>对比结果</h2>");
    html += QString("<p>修复前问题 %1 个 → 修复后问题 %2 个</p>").arg(before.size()).arg(after.size());
    html += QString("<p style='font-size:14px'>"
                    "<b style='color:#10B981'>已解决 %1</b> &nbsp; "
                    "<b style='color:#E5484D'>新增 %2</b> &nbsp; "
                    "<b style='color:#F59E0B'>持续存在 %3</b></p>")
                .arg(r.resolved.size()).arg(r.added.size()).arg(r.persistent.size());

    auto render = [](const QString& title, const QColor& c, const QVector<Anomaly>& list) {
        QString h = QString("<h3 style='color:%1'>%2（%3）</h3><ul>").arg(c.name(), title).arg(list.size());
        for (const Anomaly& a : list) {
            h += QString("<li><b>[%1]</b> %2 <span style='color:#8892A6'>(%3)</span></li>")
                     .arg(severityName(a.severity), a.title.toHtmlEscaped(), a.timeStr);
        }
        h += "</ul>";
        return h;
    };
    html += render("已解决", QColor("#10B981"), r.resolved);
    html += render("新增", QColor("#E5484D"), r.added);
    html += render("持续存在", QColor("#F59E0B"), r.persistent);
    m_result->setHtml(html);
}
