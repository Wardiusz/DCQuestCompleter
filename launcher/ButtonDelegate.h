#pragma once
#include <QStyledItemDelegate>
#include <QApplication>
#include "GameModel.h"

class ButtonDelegate final : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    auto paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const -> void override {
        const QString text = index.data(Qt::DisplayRole).toString();
        const bool isButtonText =
            text == "Start"  ||
            text == "Stop"   ||
            text == "Launch" ||
            text == "Remove";

        const bool isGameModelButton =
            !isButtonText &&
            (index.column() == GameModel::StartButton ||
             index.column() == GameModel::StopButton);

        if (isButtonText || isGameModelButton) {
            QStyleOptionButton button;
            button.rect = option.rect.adjusted(4, 4, -4, -4);
            button.text = isButtonText ? text : (index.column() == GameModel::StartButton ? "Start" : "Stop");
            button.state = QStyle::State_Enabled;

            QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }

    auto editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) -> bool override {
        if (event->type() == QEvent::MouseButtonRelease) {
            const QString text = index.data(Qt::DisplayRole).toString();

            if (text == "Start" || text == "Launch") {
                emit startClicked(index);
                return true;
            }
            if (text == "Stop" || text == "Remove") {
                emit stopClicked(index);
                return true;
            }

            if (index.column() == GameModel::StartButton) {
                emit startClicked(index);
                return true;
            }
            if (index.column() == GameModel::StopButton) {
                emit stopClicked(index);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void startClicked(const QModelIndex& index) const;
    void stopClicked(const QModelIndex& index) const;
};