#include "jsonitemdelegate.h"
#include "qjsonmodel.h"
#include "qjsonitem.h"
#include <QComboBox>
#include <QDebug>

JsonItemDelegate::JsonItemDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

QWidget *JsonItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // create an editor for the "Type" column
    if (index.column() != 1) {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    // A QComboBox for editing the type
    QComboBox *editor = new QComboBox(parent);
    editor->addItems({"String", "Double", "Bool", "Array", "Object", "Null"});
    return editor;
}

void JsonItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // If not column 1, use the base implementation
    if (index.column() != 1) {
        QStyledItemDelegate::setEditorData(editor, index);
        return;
    }

    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    // Read the item's canonical type name directly instead of parsing
    // the display string. The model's column-1 DisplayRole is
    // "Array[N]" for arrays (childcount suffix) and the bare type
    // name otherwise - using QJsonTreeItem::typeName() side-steps
    // the parse entirely and stays correct if more types are added.
    QJsonTreeItem *item = static_cast<QJsonTreeItem*>(index.internalPointer());
    if (item)
        comboBox->setCurrentText(item->typeName());
    else
        QStyledItemDelegate::setEditorData(editor, index);
}

void JsonItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    // If not column 1, use the base implementation
    if (index.column() != 1) {
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }

    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    // Set the model's data to the new type name selected in the combo box
    model->setData(index, comboBox->currentText(), Qt::EditRole);
}
