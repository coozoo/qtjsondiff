/***********************************************
    Copyright (C) 2014  Schutz Sacha
    This file is part of QJsonModel (https://github.com/dridk/QJsonmodel).

    QJsonModel is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    QJsonModel is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with QJsonModel.  If not, see <http://www.gnu.org/licenses/>.

**********************************************/

/*
 * Modified by Yuriy Kuzin
 * Updated to support diff
 *
 */

#include "qjsonmodel.h"
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QIcon>
#include <QFont>
#include <algorithm>

QJsonModel::QJsonModel(QObject *parent) :
    QAbstractItemModel(parent)
{
    mRootItem = new QJsonTreeItem;
    mHeaders.append(tr("Name"));
    mHeaders.append(tr("Type"));
    mHeaders.append(tr("Value"));
    mHasParseError = false;
    mLastErrorMessage.clear();
    mLastErrorOffset = 0;
}

QJsonModel::~QJsonModel()
{
    delete mRootItem;
}

void QJsonModel::setEditable(bool editable)
{
    mIsEditable = editable;
}

/* maybe it's not really right place
 * file opening inside model
 * but for me it's normal
 */
bool QJsonModel::load(const QString &fileName)
{
    QFile file(fileName);
    bool success = false;
    if (file.open(QIODevice::ReadOnly)) {
        success = load(&file);
        file.close();
    }
    else success = false;

    return success;
}

/* read everything from device
 */
bool QJsonModel::load(QIODevice *device)
{
    return loadJson(device->readAll());
}

/* Load json into model from text
 * emit signal when new model loaded - dataUpdated()
 * so it's possible to listen it
 */
bool QJsonModel::loadJson(const QByteArray &json)
{

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        setParseError(true, parseError.errorString(), parseError.offset);
        beginResetModel();
        delete mRootItem;
        mRootItem = QJsonTreeItem::load(QJsonValue(QJsonDocument::fromJson((QString(
                                                                            "{\"Error\":\"") + QString(parseError.errorString() +
                                                                            "\",\"offset\":")+ QString::number(parseError.offset) +
                                                                            "}").toUtf8()).object()));
        endResetModel();
        emit dataUpdated();
        return true;
    }
    else
    {
        setParseError(false, QString(), 0);
    }
    if(!doc.isNull())
    {
        beginResetModel();
        delete mRootItem;
        if(doc.isObject())
        {
            mRootItem = QJsonTreeItem::load(QJsonValue(doc.object()));
            mRootItem->setType(QJsonValue::Object);
        }
        else
        {
            mRootItem = QJsonTreeItem::load(QJsonValue(doc.array()));
            mRootItem->setType(QJsonValue::Array);
        }
        endResetModel();
        emit dataUpdated();
        return true;
    }
    else
    {
        qDebug()<<"Error!! in json";
        emit dataUpdated();
        return false;
    }
}

void QJsonModel::setParseError(bool hasError, const QString& msg, int offset)
{
    if (mHasParseError != hasError || mLastErrorMessage != msg || mLastErrorOffset != offset)
    {
        mHasParseError = hasError;
        mLastErrorMessage = msg;
        mLastErrorOffset = offset;
        emit parseErrorChanged();
    }
}


QVariant QJsonModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();


    QJsonTreeItem *item = static_cast<QJsonTreeItem*>(index.internalPointer());

    if (role == Qt::BackgroundRole)
    {
        if(item->colorType() == DiffColorType::None)
        {
            // Let's not use white color. It breaks dark themes
            // and alternating background row colors too.
            return QPalette::Base;
        }
        else
        {
            return item->color();
        }
    }

    if (role==Qt::ToolTipRole)
    {
        return item->toolTip();
    }

    if ((role == Qt::DecorationRole) && (index.column() == 0)){
        return mTypeIcons.value(item->type());
    }

    if (role == Qt::ForegroundRole) {
        return QPalette::Text; // Theme color for text
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == 0)
            return QString("%1").arg(item->key());
        if (index.column() == 1)
        {
            if (item->type()==QJsonValue::Array)
            {
                return QString("Array[%1]").arg(item->childCount());
            }
            else
            {
                return QString("%1").arg(item->typeName());
            }
        }

        if (index.column() == 2)
            return QString("%1").arg(item->value());
    }

    return QVariant();
}

bool QJsonModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid())
        return false;

    QJsonTreeItem *item = itemFromIndex(index);
    const QString valStr = value.toString();

    // Handle column 1: Type change
    if (index.column() == 1)
    {
        const QJsonValue::Type oldType = item->type();
        const QString& newTypeName = valStr;
        QJsonValue::Type newType = QJsonTreeItem::stringToType(newTypeName);

        if (newType == oldType) return true;

        // Take ownership of children before clearing them, so we can reuse them
        QList<QJsonTreeItem*> children;
        if(item->childCount() > 0) {
            beginRemoveRows(index, 0, item->childCount() - 1);
            children = item->takeChildren();
            endRemoveRows();
        }

        QString newValue = "";
        switch(newType) {
            case QJsonValue::Double:
                if (oldType == QJsonValue::Bool) {
                    newValue = (item->value().toLower() == "true") ? "1" : "0";
                } else if (oldType == QJsonValue::String) {
                    if (item->value().toLower() == "true") {
                        newValue = "1";
                    } else if (item->value().toLower() == "false") {
                        newValue = "0";
                    } else {
                        newValue = QString::number(item->value().toDouble());
                    }
                } else {
                    newValue = QString::number(item->value().toDouble());
                }
                qDeleteAll(children);
                children.clear();
                break;
            case QJsonValue::String:
                newValue = (item->type() == QJsonValue::Null) ? "" : item->value();
                qDeleteAll(children);
                children.clear();
                break;
            case QJsonValue::Bool:
                newValue = (item->value().toLower() == "true" || item->value().toInt() != 0) ? "true" : "false";
                qDeleteAll(children);
                children.clear();
                break;
            case QJsonValue::Null:
                newValue = "";
                qDeleteAll(children);
                children.clear();
                break;

            case QJsonValue::Object: {
                if (oldType == QJsonValue::Array) {
                    bool isKeyValuePairs = !children.isEmpty();
                    for(QJsonTreeItem* child : children) {
                        if (child->type() != QJsonValue::Array || child->childCount() != 2) {
                            isKeyValuePairs = false;
                            break;
                        }
                    }

                    if (isKeyValuePairs) {
                        // Case: [["bar", "car"]] -> {"bar": "car"}
                        if (!children.isEmpty())
                           beginInsertRows(index, 0, children.count() - 1);
                        for (QJsonTreeItem* pairArray : children) {
                            QJsonTreeItem* valItem = pairArray->child(1);
                            valItem->setKey(pairArray->child(0)->value());
                            pairArray->takeChildren(); // Prevent valItem from being deleted with pairArray
                            item->appendChild(valItem);
                            delete pairArray;
                        }
                         if (!children.isEmpty())
                           endInsertRows();

                    } else {
                        // Case: ["ar", "bg"] -> {"0": "ar", "1": "bg"}
                        if (!children.isEmpty())
                           beginInsertRows(index, 0, children.count() - 1);
                        for (int i = 0; i < children.count(); ++i) {
                            QJsonTreeItem* child = children.at(i);
                            child->setKey(QString::number(i));
                            item->appendChild(child);
                        }
                        if (!children.isEmpty())
                           endInsertRows();
                    }
                } else if(!children.isEmpty()) {
                    beginInsertRows(index, 0, children.count() - 1);
                    item->setChildren(children);
                    for (QJsonTreeItem* child : children) child->setParent(item);
                    endInsertRows();
                }
                children.clear(); // Children have been re-parented or deleted
                break;
            }

            case QJsonValue::Array: {
                if (oldType == QJsonValue::Object) {
                    std::sort(children.begin(), children.end(), [](QJsonTreeItem* a, QJsonTreeItem* b) {
                        bool aOk, bOk;
                        int aKey = a->key().toInt(&aOk);
                        int bKey = b->key().toInt(&bOk);
                        if (aOk && bOk) {
                            return aKey < bKey;
                        }
                        return a->key() < b->key(); // Fallback for non-numeric keys
                    });

                    bool isSequentialKeys = !children.isEmpty();
                    for(int i = 0; i < children.count(); ++i) {
                        bool ok;
                        if(children.at(i)->key().toInt(&ok) != i || !ok) {
                            isSequentialKeys = false;
                            break;
                        }
                    }

                    if (isSequentialKeys) {
                        // Case: {"0": "ar", "1": "bg"} -> ["ar", "bg"]
                        if (!children.isEmpty())
                            beginInsertRows(index, 0, children.count() - 1);
                        for (int i = 0; i < children.count(); ++i) {
                            QJsonTreeItem* child = children.at(i);
                            child->setKey(QString::number(i));
                            item->appendChild(child);
                        }
                        if (!children.isEmpty())
                            endInsertRows();
                    } else {
                        // Case: {"bar": "car"} -> [["bar", "car"]]
                         if (!children.isEmpty())
                            beginInsertRows(index, 0, children.count() - 1);
                        for (int i=0; i < children.count(); ++i) {
                            QJsonTreeItem* oldChild = children.at(i);
                            oldChild->setParent(nullptr);

                            auto* newParentArray = new QJsonTreeItem();
                            newParentArray->setType(QJsonValue::Array);
                            newParentArray->setKey(QString::number(i));

                            auto* keyItem = new QJsonTreeItem();
                            keyItem->setKey("0");
                            keyItem->setType(QJsonValue::String);
                            keyItem->setValue(oldChild->key());

                            oldChild->setKey("1");

                            newParentArray->appendChild(keyItem);
                            newParentArray->appendChild(oldChild);
                            item->appendChild(newParentArray);
                        }
                         if (!children.isEmpty())
                            endInsertRows();
                    }
                    children.clear();


                } else if (!children.isEmpty()) {
                    beginInsertRows(index, 0, children.count() - 1);
                    item->setChildren(children);
                    for (QJsonTreeItem* child : children) child->setParent(item);
                    endInsertRows();
                    children.clear();
                }
                break;
            }
            default:
                break;
        }

        item->setType(newType);
        item->setValue(newValue);

        qDeleteAll(children); // Delete any remaining children that were not re-parented

        emit dataChanged(index.siblingAtColumn(0), index.siblingAtColumn(columnCount() - 1), {Qt::DisplayRole, Qt::EditRole});
        emit modelChanged();
        return true;
    }

    // Handle columns 0 (Key) and 2 (Value)
    switch(index.column()) {
        case 0:
            item->setKey(valStr);
            break;
        case 2:
            item->setValue(valStr);
            break;
        default:
            return false;
    }

    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    emit modelChanged();
    return true;
}

Qt::ItemFlags QJsonModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    if (!mIsEditable)
        return flags;

    QJsonTreeItem *item = static_cast<QJsonTreeItem*>(index.internalPointer());
    
    // Type is editable, but not for the root item
    if (index.column() == 1 && item->parent()) {
        flags |= Qt::ItemIsEditable;
    }
    
    // Keys are not editable for array elements
    if (index.column() == 0 && item->parent() && item->parent()->type() != QJsonValue::Array) {
        flags |= Qt::ItemIsEditable;
    }

    // Values are not editable for objects, arrays, or nulls
    if (index.column() == 2 &&
        item->type() != QJsonValue::Object &&
        item->type() != QJsonValue::Array &&
        item->type() != QJsonValue::Null) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant QJsonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        return mHeaders.value(section);
    }
    else
        return QVariant();
}

QModelIndex QJsonModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    QJsonTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = mRootItem;
    else
        parentItem = static_cast<QJsonTreeItem*>(parent.internalPointer());

    QJsonTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex QJsonModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    QJsonTreeItem *childItem = static_cast<QJsonTreeItem*>(index.internalPointer());
    QJsonTreeItem *parentItem = childItem->parent();

    if (parentItem == mRootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

QJsonTreeItem* QJsonModel::itemFromIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<QJsonTreeItem*>(index.internalPointer());
}

int QJsonModel::rowCount(const QModelIndex &parent) const
{
    QJsonTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = mRootItem;
    else
        parentItem = static_cast<QJsonTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int QJsonModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return mHeaders.count();
}

void QJsonModel::setIcon(const QJsonValue::Type &type, const QIcon &icon)
{
    mTypeIcons.insert(type,icon);
}

QList<QModelIndex> QJsonModel::parents(QModelIndex &index) const
{
    QList<QModelIndex> parents=QList<QModelIndex>();
    if (!index.isValid())
        return  parents;

    QJsonTreeItem *current_item = static_cast<QJsonTreeItem*>(index.internalPointer());
    QJsonTreeItem *current_parent = current_item->parent();

    while(current_parent!=mRootItem)
    {
        parents.append(createIndex(current_parent->row(), 0, current_parent));
        current_item = current_parent;
        current_parent = current_item->parent();
    }
    parents.append(createIndex(mRootItem->row(), 0, mRootItem));
    return parents;
}

QList<QModelIndex> QJsonModel::jsonIndexPath(QModelIndex &index) const
{
    QList<QModelIndex> parents=this->parents(index);
    for (int k = 0; k < (parents.size() / 2); ++k)
    {
        std::swap(parents[k], parents[parents.size() - 1 - k]);
    }
    QJsonTreeItem *current_item = static_cast<QJsonTreeItem*>(index.internalPointer());
    parents.append(createIndex(current_item->row(), 0, current_item));
    return parents;
}

QString QJsonModel::jsonPath(QModelIndex &index) const
{
    QList<QModelIndex> parents=this->jsonIndexPath(index);

    QString ret="";
    for( int i=0; i<parents.count(); ++i )
    {
        ret.append( parents[i].data(Qt::DisplayRole).toString()+"("+static_cast<QJsonTreeItem*>(parents[i].internalPointer())->typeName()+")->");
    }
    return ret.left(ret.length()-2);
}

void QJsonModel::generateJson(QJsonTreeItem *item, QJsonValue &value) const {
    const int childCount = item->childCount();
    if (item->type() == QJsonValue::Object) {
        QJsonObject obj;
        for (int i = 0; i < childCount; ++i) {
            QJsonTreeItem *child = item->child(i);
            QJsonValue childValue;
            generateJson(child, childValue);
            obj.insert(child->key(), childValue);
        }
        value = obj;
    } else if (item->type() == QJsonValue::Array) {
        QJsonArray arr;
        for (int i = 0; i < childCount; ++i) {
            QJsonTreeItem *child = item->child(i);
            QJsonValue childValue;
            generateJson(child, childValue);
            arr.append(childValue);
        }
        value = arr;
    } else {
        const QString &valStr = item->value();
        switch (item->type()) {
            case QJsonValue::Bool:
                value = (valStr.toLower() == "true");
                break;
            case QJsonValue::Double:
                value = valStr.toDouble();
                break;
            case QJsonValue::String:
                value = valStr;
                break;
            case QJsonValue::Null:
            default:
                value = QJsonValue::Null;
                break;
        }
    }
}

QJsonValue::Type QJsonModel::rootType() const
{
    if (!mRootItem)
        return QJsonValue::Null;
    return mRootItem->type();
}

QJsonDocument QJsonModel::getJsonDocument() const {
    if (!mRootItem)
        return QJsonDocument();

    QJsonValue rootValue;
    generateJson(mRootItem, rootValue);

    if (mRootItem->type() == QJsonValue::Object) {
        return QJsonDocument(rootValue.toObject());
    } else if (mRootItem->type() == QJsonValue::Array) {
        return QJsonDocument(rootValue.toArray());
    }

    return QJsonDocument();
}
