#pragma once
#include <QStandardItemModel>
#include <QJsonArray>
#include <QJsonObject>

class GameModel final : public QStandardItemModel {
    Q_OBJECT
public:
    enum Columns {
        Name = 0,
        Id,
        StartButton,
        StopButton,
        ColCount
    };

    explicit GameModel(QObject* parent = nullptr) : QStandardItemModel(parent) {
        setupHeaders();
    }

    auto loadFromJsonArray(const QJsonArray& arr) -> void {
        clear();
        setupHeaders();

        for (const auto& v : arr) {
            const auto obj = v.toObject();
            const auto name = obj.value("name").toString();
            const auto id = QString::number(obj.value("id").toVariant().toLongLong());

            if (name.isEmpty() || id.isEmpty()) continue;

            m_nameItem = new QStandardItem(name);
            m_idItem = new QStandardItem(id);
            m_startItem = new QStandardItem();
            m_stopItem = new QStandardItem();

            m_nameItem->setEditable(false);
            m_idItem->setEditable(false);
            m_startItem->setEditable(false);
            m_stopItem->setEditable(false);

            m_nameItem->setData(obj, Qt::UserRole);

            QList<QStandardItem*> row;
            row << m_nameItem << m_idItem << m_startItem << m_stopItem;
            appendRow(row);
        }
    }

    auto gameObject(const int row) const -> QJsonObject {
        if (row < 0 || row >= rowCount())
            return {};
        const auto* item = this->item(row, Name);
        if (!item)
            return {};
        return item->data(Qt::UserRole).toJsonObject();
    }

private:
    QStandardItem* m_nameItem = nullptr;
    QStandardItem* m_idItem = nullptr;
    QStandardItem* m_startItem = nullptr;
    QStandardItem* m_stopItem = nullptr;


    auto setupHeaders() -> void {
        setColumnCount(ColCount);
        setHeaderData(Name, Qt::Horizontal, "Name");
        setHeaderData(Id, Qt::Horizontal, "ID");
        setHeaderData(StartButton, Qt::Horizontal, "Action");
        setHeaderData(StopButton, Qt::Horizontal, "Stop");
    }
};
