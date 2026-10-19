#pragma once
#include <QWidget>
#include "core/timeline.h"

class QTableView;
class QComboBox;
class LogTableModel;

// 原始日志表格：按节点类型过滤查看
class LogTableView : public QWidget {
    Q_OBJECT
public:
    explicit LogTableView(QWidget* parent = nullptr);
    void setTimeline(const Timeline* tl);

private slots:
    void onFilterChanged(int);

private:
    QComboBox* m_filter = nullptr;
    QTableView* m_table = nullptr;
    LogTableModel* m_model = nullptr;
};
