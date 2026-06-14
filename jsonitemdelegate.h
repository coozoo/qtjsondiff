#ifndef JSONITEMDELEGATE_H
#define JSONITEMDELEGATE_H

#include <QStyledItemDelegate>

class JsonItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit JsonItemDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
};

#endif // JSONITEMDELEGATE_H
