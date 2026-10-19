#pragma once
#include <QDialog>
#include <QVector>
#include "analyze/anomaly.h"

class QLineEdit;
class QTextBrowser;

// 修复前后报告对比对话框
class CompareDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompareDialog(QWidget* parent = nullptr);

private slots:
    void browseBefore();
    void browseAfter();
    void runCompare();

private:
    QLineEdit* m_before = nullptr;
    QLineEdit* m_after = nullptr;
    QTextBrowser* m_result = nullptr;
};
