#ifndef SHORTCUTDELEGATE_H
#define SHORTCUTDELEGATE_H

#include <QStyledItemDelegate>
#include <QKeySequenceEdit>

class ShortcutDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ShortcutDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        auto *editor = new QKeySequenceEdit(parent);
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *keyEdit = static_cast<QKeySequenceEdit*>(editor);
        QKeySequence keySequence = index.model()->data(index, Qt::EditRole).value<QKeySequence>();
        keyEdit->setKeySequence(keySequence);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        auto *keyEdit = static_cast<QKeySequenceEdit*>(editor);
        model->setData(index, keyEdit->keySequence(), Qt::EditRole);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index);
        editor->setGeometry(option.rect);
    }
};

#endif // SHORTCUTDELEGATE_H
