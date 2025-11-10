#include "jsonitemdelegate.h"
#include "qjsonmodel.h"
#include <QComboBox>
#include <QDebug>

JsonItemDelegate::JsonItemDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

QWidget *JsonItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Only create an editor for the "Type" column
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
    // The current value is the item's type name
    QString currentType = index.model()->data(index, Qt::DisplayRole).toString();
    // For arrays, the display role is e.g. "Array[5]", so we strip it.
    if (currentType.startsWith("Array")) {
        currentType = "Array";
    }
    comboBox->setCurrentText(currentType);
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
