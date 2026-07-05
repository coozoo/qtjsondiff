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
#include <QMimeData>
#include <QSet>
#include <algorithm>

const QString QJsonModel::kItemMimeType =
        QStringLiteral("application/x-qjsondiff-item");

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
        if (item->isPhantom())
            return QVariant();
        return mTypeIcons.value(item->type());
    }

    if (role == Qt::ForegroundRole) {
        return QPalette::Text; // Theme color for text
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // Phantoms are alignment-only spacer rows. Render as blank
        // in every column so they can't be mistaken for real content
        // (a phantom under an Array would otherwise show its positional
        // "0"/"1"/... as if it were a real index).
        if (item->isPhantom())
            return QString();
        if (index.column() == 0)
            return QString("%1").arg(item->key());
        if (index.column() == 1)
        {
            if (item->type()==QJsonValue::Array)
            {
                int real = 0;
                for (int i = 0; i < item->childCount(); ++i)
                    if (!item->child(i)->isPhantom())
                        ++real;
                return QString("Array[%1]").arg(real);
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
    if (item && item->isPhantom())
        return false;
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
        // Structural change — children may have been removed/inserted
        // by the Object<->Array conversion logic above. Tell listeners
        // (QJsonContainer) to reset their cached find/goto QModelIndex
        // lists. Same reason as in replaceFromJson().
        emit dataUpdated();
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
    {
        // Invalid index = the root. Qt consults flags(QModelIndex())
        // when deciding whether a drop on empty viewport space is OK.
        // Allow root-level drops only when the root is a container and
        // the model is editable; otherwise no flags (default behavior).
        if (mIsEditable && mRootItem
                && (mRootItem->type() == QJsonValue::Object
                    || mRootItem->type() == QJsonValue::Array))
            return Qt::ItemIsDropEnabled;
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    if (!mIsEditable)
        return flags;

    QJsonTreeItem *item = static_cast<QJsonTreeItem*>(index.internalPointer());

    // Phantoms are read-only placeholders; strip Selectable/Enabled?
    // No — the view still needs to size and paint them. Just deny edit.
    if (item->isPhantom())
        return flags;

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

    // Drag enabled for any non-root item (root cannot leave the tree).
    if (item->parent())
        flags |= Qt::ItemIsDragEnabled;

    // Drop enabled when the item is a container — it can receive
    // children. The root container is handled via the invalid parent
    // branch in canDropMimeData().
    if (item->type() == QJsonValue::Object || item->type() == QJsonValue::Array)
        flags |= Qt::ItemIsDropEnabled;

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
            if (child->isPhantom()) continue;
            QJsonValue childValue;
            generateJson(child, childValue);
            obj.insert(child->key(), childValue);
        }
        value = obj;
    } else if (item->type() == QJsonValue::Array) {
        QJsonArray arr;
        for (int i = 0; i < childCount; ++i) {
            QJsonTreeItem *child = item->child(i);
            if (child->isPhantom()) continue;
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

bool QJsonModel::replaceFromJson(const QModelIndex &target, const QJsonValue &source)
{
    if (!target.isValid() || source.isUndefined())
        return false;
    QJsonTreeItem *item = itemFromIndex(target);
    if (!item)
        return false;

    // begin/endRemove and begin/endInsert use the column-0 sibling as
    // the parent index, per Qt model contract.
    const QModelIndex parent0 = target.siblingAtColumn(0);

    // 1) drop any existing children with proper structural signals so
    //    selection/expansion state in views stays correct
    if (item->childCount() > 0)
    {
        beginRemoveRows(parent0, 0, item->childCount() - 1);
        item->clearChildren();
        endRemoveRows();
    }

    // 2) overwrite type + value to match the source
    item->setType(source.type());
    if (source.isString())
        item->setValue(source.toString());
    else if (source.isDouble())
        item->setValue(QString::number(source.toDouble()));
    else if (source.isBool())
        item->setValue(source.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    else
        item->setValue(QString());  // Object/Array/Null carry empty value strings

    // 3) for containers, build a fresh subtree from source and steal
    //    its children. We use QJsonTreeItem::load() so child key
    //    numbering / type assignment matches the loadJson() path.
    if (source.isObject() || source.isArray())
    {
        QJsonTreeItem *built = QJsonTreeItem::load(source);
        QList<QJsonTreeItem*> kids = built->takeChildren();
        if (!kids.isEmpty())
        {
            beginInsertRows(parent0, 0, kids.size() - 1);
            for (QJsonTreeItem *kid : kids)
                item->appendChild(kid);
            endInsertRows();
        }
        delete built;  // wrapper drained of children, safe to drop
    }

    emit dataChanged(parent0,
                     target.siblingAtColumn(columnCount() - 1),
                     {Qt::DisplayRole, Qt::EditRole, Qt::DecorationRole});
    emit modelChanged();
    // Structural change — children were removed/inserted. Tell listeners
    // (QJsonContainer) to reset cached find/goto QModelIndex lists and
    // refresh the diff counter, the same way they react to loadJson.
    emit dataUpdated();
    return true;
}

QModelIndex QJsonModel::appendChildFromJson(const QModelIndex &parent,
                                            const QString &key,
                                            const QJsonValue &source)
{
    if (source.isUndefined())
        return QModelIndex();

    QJsonTreeItem *parentItem = parent.isValid() ? itemFromIndex(parent)
                                                 : mRootItem;
    if (!parentItem)
        return QModelIndex();
    if (parentItem->type() != QJsonValue::Object
            && parentItem->type() != QJsonValue::Array)
        return QModelIndex();

    // Object children need a non-empty key. QJsonObject::insert("", v)
    // is legal but every subsequent empty-key insert overwrites the
    // prior one on serialize, silently dropping siblings. Refuse here
    // so integrators don't corrupt JSON by accident; arrays override
    // the key with the row index a few lines below, so they're unaffected.
    if (parentItem->type() == QJsonValue::Object && key.isEmpty())
    {
        qWarning() << "QJsonModel::appendChildFromJson: empty key for "
                      "Object parent — rejecting to avoid silent "
                      "QJsonObject::insert overwrite on serialize.";
        return QModelIndex();
    }

    // load() returns a wrapper whose own slot represents `source`:
    //  - scalar: type+value set on the wrapper directly; no children.
    //  - container: children built recursively; type left unset on the
    //    wrapper (loadJson() patches that — we do the same here).
    QJsonTreeItem *newChild = QJsonTreeItem::load(source);
    newChild->setType(source.type());
    newChild->setKey(parentItem->type() == QJsonValue::Array
                         ? QString::number(parentItem->childCount())
                         : key);

    const int row = parentItem->childCount();
    const QModelIndex parent0 = parent.isValid() ? parent.siblingAtColumn(0)
                                                 : QModelIndex();

    beginInsertRows(parent0, row, row);
    parentItem->appendChild(newChild);
    endInsertRows();

    // Parent's childCount text (column 2) may change; refresh its row.
    if (parent.isValid())
    {
        emit dataChanged(parent.siblingAtColumn(0),
                         parent.siblingAtColumn(columnCount() - 1),
                         {Qt::DisplayRole, Qt::EditRole, Qt::DecorationRole});
    }
    emit modelChanged();
    emit dataUpdated();

    return index(row, 0, parent0);
}

bool QJsonModel::removeRowAt(const QModelIndex &target)
{
    if (!target.isValid())
        return false;
    QJsonTreeItem *item = itemFromIndex(target);
    if (!item || item == mRootItem)
        return false;
    QJsonTreeItem *parentItem = item->parent();
    if (!parentItem)
        return false;

    const int row = item->row();
    const QModelIndex parent0 = target.parent();

    beginRemoveRows(parent0, row, row);
    // Single-index takeChildAt keeps mChilds atomically off-by-one
    // during the transaction (as opposed to the older takeChildren/
    // setChildren round-trip, which briefly emptied the list — a
    // window that would let a proxy or nested-event-loop observer
    // see rowCount()==0 mid-remove and violate the QAIM contract).
    QJsonTreeItem *removed = parentItem->takeChildAt(row);
    delete removed;
    endRemoveRows();

    // Array children carry stringified-index keys ("0","1","2"…). After
    // removing one, the survivors past the removal point keep their
    // original strings, so the display would show "0","2","3" with a
    // gap. getJsonDocument() serialises by position so the JSON is fine,
    // but the user sees inconsistent keys. Renumber from `row` onward
    // and emit dataChanged for the key column.
    if (parentItem->type() == QJsonValue::Array)
    {
        for (int i = row; i < parentItem->childCount(); ++i)
        {
            QJsonTreeItem *sibling = parentItem->child(i);
            const QString want = QString::number(i);
            if (sibling->key() != want)
            {
                sibling->setKey(want);
                QModelIndex keyIdx = index(i, 0, parent0);
                emit dataChanged(keyIdx, keyIdx,
                                 {Qt::DisplayRole, Qt::EditRole});
            }
        }
        // Array parent's column 1 shows "Array[N]" — N just changed.
        // Refresh so views without a full-tree repaint pick up the new
        // count. Mirrors what appendChildFromJson / insertChildFromJson
        // already do on the insert side.
        if (parent0.isValid())
        {
            const QModelIndex typeIdx = parent0.siblingAtColumn(1);
            emit dataChanged(typeIdx, typeIdx,
                             {Qt::DisplayRole, Qt::EditRole});
        }
    }

    emit modelChanged();
    emit dataUpdated();
    return true;
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

QModelIndex QJsonModel::insertChildFromJson(const QModelIndex &parent,
                                            int row,
                                            const QString &key,
                                            const QJsonValue &source)
{
    if (source.isUndefined())
        return QModelIndex();

    QJsonTreeItem *parentItem = parent.isValid() ? itemFromIndex(parent)
                                                 : mRootItem;
    if (!parentItem)
        return QModelIndex();
    if (parentItem->type() != QJsonValue::Object
            && parentItem->type() != QJsonValue::Array)
        return QModelIndex();

    const int count = parentItem->childCount();
    if (row < 0 || row > count) row = count;

    // Append fast-path delegates to appendChildFromJson for the trivial
    // case and for object parents — object children are unordered by
    // key, so insert-at-row is the same as append plus a meaningless
    // visual position change.
    if (row == count || parentItem->type() == QJsonValue::Object)
        return appendChildFromJson(parent, key, source);

    // Array mid-insert: build the new child, insert at row, then renumber
    // siblings past the insert point so the displayed keys stay 0..N-1.
    QJsonTreeItem *newChild = QJsonTreeItem::load(source);
    newChild->setType(source.type());
    newChild->setKey(QString::number(row));

    const QModelIndex parent0 = parent.isValid() ? parent.siblingAtColumn(0)
                                                 : QModelIndex();

    beginInsertRows(parent0, row, row);
    // Single-index insertChild keeps mChilds atomically off-by-one
    // during the transaction. See removeRowAt for the same rationale.
    parentItem->insertChild(row, newChild);
    endInsertRows();

    // Renumber siblings past the insert point so the key column matches
    // their new positions. Mirrors removeRowAt's renumber loop.
    for (int i = row + 1; i < parentItem->childCount(); ++i)
    {
        QJsonTreeItem *sibling = parentItem->child(i);
        const QString want = QString::number(i);
        if (sibling->key() != want)
        {
            sibling->setKey(want);
            QModelIndex keyIdx = index(i, 0, parent0);
            emit dataChanged(keyIdx, keyIdx,
                             {Qt::DisplayRole, Qt::EditRole});
        }
    }

    if (parent.isValid())
    {
        emit dataChanged(parent.siblingAtColumn(0),
                         parent.siblingAtColumn(columnCount() - 1),
                         {Qt::DisplayRole, Qt::EditRole, Qt::DecorationRole});
    }
    emit modelChanged();
    emit dataUpdated();
    return index(row, 0, parent0);
}

QJsonTreeItem *QJsonModel::insertPhantomRow(const QModelIndex &parent, int row)
{
    QJsonTreeItem *parentItem = parent.isValid() ? itemFromIndex(parent)
                                                  : mRootItem;
    if (!parentItem)
        return nullptr;
    const int count = parentItem->childCount();
    if (row < 0 || row > count) row = count;

    // Column-0 form of `parent` — beginInsertRows follows Qt convention
    // that structural signals reference the column-0 index of the
    // parent (matches insertChildFromJson above).
    const QModelIndex parent0 = parent.isValid() ? parent.siblingAtColumn(0)
                                                 : QModelIndex();

    beginInsertRows(parent0, row, row);
    auto *ph = new QJsonTreeItem(parentItem);
    ph->setKey(QString::number(row));
    ph->setType(QJsonValue::Null);
    ph->setValue(QString());
    ph->setPhantom(true);
    ph->setColorType(DiffColorType::NotPresent);
    parentItem->insertChild(row, ph);
    endInsertRows();
    return ph;
}

// ---------------------------------------------------------------------
// Drag-and-drop
// ---------------------------------------------------------------------

Qt::DropActions QJsonModel::supportedDragActions() const
{
    // Copy only — the source side keeps its node. The user's mental
    // model for diff DnD is "duplicate this onto the other side",
    // not "move it" (which would leave a hole and break ordering on
    // the source tree).
    return Qt::CopyAction;
}

Qt::DropActions QJsonModel::supportedDropActions() const
{
    return Qt::CopyAction;
}

QStringList QJsonModel::mimeTypes() const
{
    // Deliberately omit "text/uri-list". File-URL drags must fall
    // through the tree (which ignores unrecognized MIME) up to the
    // surrounding QGroupBox where the file-drop event filter lives.
    return QStringList{kItemMimeType};
}

QMimeData *QJsonModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty()) return nullptr;

    // Single-item drag only — the tree's selection is single by default
    // and packing multiple at once raises insertion-order questions we
    // don't want to answer for v1. Pick the first column-0 sibling.
    QModelIndex src;
    for (const QModelIndex &i : indexes)
    {
        if (i.isValid() && i.column() == 0) { src = i; break; }
    }
    if (!src.isValid()) src = indexes.first();
    if (!src.isValid()) return nullptr;

    QJsonTreeItem *item = itemFromIndex(src);
    if (!item || item == mRootItem) return nullptr;

    // Serialise the subtree to a QJsonValue using the same path
    // getJsonDocument() uses. Scalars come through with their literal
    // value; containers come through with their nested structure.
    QJsonValue payload;
    generateJson(item, payload);

    QJsonObject env;
    env.insert(QStringLiteral("key"),   item->key());
    env.insert(QStringLiteral("value"), payload);

    QMimeData *md = new QMimeData;
    md->setData(kItemMimeType,
                QJsonDocument(env).toJson(QJsonDocument::Compact));
    return md;
}

bool QJsonModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int row, int column,
                                 const QModelIndex &parent) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    if (!mIsEditable) return false;
    if (!data || !data->hasFormat(kItemMimeType)) return false;
    if (action != Qt::CopyAction && action != Qt::IgnoreAction) return false;

    // Drop parent must be a container. Invalid parent = root drop;
    // accept only if root is a container.
    if (parent.isValid())
    {
        QJsonTreeItem *p = itemFromIndex(parent);
        if (!p) return false;
        return p->type() == QJsonValue::Object
                || p->type() == QJsonValue::Array;
    }
    return mRootItem
            && (mRootItem->type() == QJsonValue::Object
                || mRootItem->type() == QJsonValue::Array);
}

bool QJsonModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                              int row, int column,
                              const QModelIndex &parent)
{
    Q_UNUSED(column);
    if (!canDropMimeData(data, action, row, column, parent)) return false;
    if (action == Qt::IgnoreAction) return true;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data->data(kItemMimeType), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject env = doc.object();
    const QJsonValue payload = env.value(QStringLiteral("value"));
    if (payload.isUndefined()) return false;
    QString key = env.value(QStringLiteral("key")).toString();

    // Resolve the parent item to decide insertion semantics.
    QJsonTreeItem *parentItem = parent.isValid() ? itemFromIndex(parent)
                                                 : mRootItem;
    if (!parentItem) return false;

    // For object parents we need a key that doesn't collide. Take the
    // source key as a starting suggestion; if empty (e.g. dragged from
    // an array source where keys are positional) fall back to
    // "new_key"; then dedupe against existing children.
    if (parentItem->type() == QJsonValue::Object)
    {
        if (key.isEmpty()) key = QStringLiteral("new_key");
        QSet<QString> used;
        for (int i = 0; i < parentItem->childCount(); ++i)
            used.insert(parentItem->child(i)->key());
        if (used.contains(key))
        {
            const QString base = key;
            for (int k = 1; ; ++k)
            {
                key = QStringLiteral("%1_%2").arg(base).arg(k);
                if (!used.contains(key)) break;
            }
        }
    }

    // row == -1 means "drop on parent" (no specific row). Append.
    if (row < 0) row = parentItem->childCount();

    QModelIndex newIdx = insertChildFromJson(parent, row, key, payload);
    return newIdx.isValid();
}
