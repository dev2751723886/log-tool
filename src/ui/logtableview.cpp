#include "ui/logtableview.h"
#include "ui/theme.h"
#include <QTableView>
#include <QComboBox>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QAbstractTableModel>

class LogTableModel : public QAbstractTableModel {
public:
    explicit LogTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}
    void setTimeline(const Timeline* tl) { beginResetModel(); m_tl = tl; rebuild(); endResetModel(); }
    void setFilter(LogType t) { beginResetModel(); m_filter = t; rebuild(); endResetModel(); }
    int rowCount(const QModelIndex& = QModelIndex()) const override { return m_idx.size(); }
    int columnCount(const QModelIndex& = QModelIndex()) const override { return 4; }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!m_tl || index.row() >= m_idx.size()) return QVariant();
        const LogEntry& e = m_tl->entries()[m_idx[index.row()]];
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0: return e.timeStr;
            case 1: return e.node;
            case 2: return e.level;
            case 3: return e.message;
            }
        } else if (role == Qt::ForegroundRole) {
            if (index.column() == 2) return Theme::levelColor(e.level);
        }
        return QVariant();
    }
    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole) return QVariant();
        if (o == Qt::Horizontal) {
            switch (section) {
            case 0: return "时间";
            case 1: return "节点";
            case 2: return "级别";
            case 3: return "消息";
            }
        }
        return section + 1;
    }
private:
    void rebuild() {
        m_idx.clear();
        if (!m_tl) return;
        for (int i = 0; i < m_tl->count(); ++i) {
            const LogEntry& e = m_tl->entries()[i];
            if (m_filter != LogType::Unknown && e.type != m_filter) continue;
            m_idx.append(i);
        }
    }
    const Timeline* m_tl = nullptr;
    QVector<int> m_idx;
    LogType m_filter = LogType::Unknown;
};

LogTableView::LogTableView(QWidget* parent) : QWidget(parent) {
    m_filter = new QComboBox(this);
    m_filter->addItem("全部类型", (int)LogType::Unknown);
    for (int t = 1; t < (int)LogType::Count; ++t) {
        m_filter->addItem(logTypeLabel((LogType)t), t);
    }
    m_table = new QTableView(this);
    m_model = new LogTableModel(this);
    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 150);
    m_table->setColumnWidth(1, 140);
    m_table->setColumnWidth(2, 60);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_filter);
    lay->addWidget(m_table);
    connect(m_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogTableView::onFilterChanged);
}

void LogTableView::setTimeline(const Timeline* tl) {
    m_model->setTimeline(tl);
}

void LogTableView::onFilterChanged(int) {
    m_model->setFilter((LogType)m_filter->currentData().toInt());
}
