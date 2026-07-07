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

#include "qjsonitem.h"
#include "preferences/preferences.h"


QJsonTreeItem::QJsonTreeItem(QJsonTreeItem *parent)
{
    mParent = parent;
}

QJsonTreeItem::~QJsonTreeItem()
{
    clearChildren();
}

void QJsonTreeItem::clearChildren()
{
    qDeleteAll(mChilds);
    mChilds.clear();
}

QList<QJsonTreeItem *> QJsonTreeItem::takeChildren()
{
    QList<QJsonTreeItem*> children = mChilds;
    mChilds.clear();
    return children;
}

void QJsonTreeItem::setChildren(const QList<QJsonTreeItem *> &children)
{
    mChilds = children;
    // Reparent every child. Several callers (insertChildFromJson,
    // type-conversion paths in QJsonModel::setData) hand us a list
    // that mixes existing children (whose mParent is already `this`)
    // with freshly-built ones (mParent = nullptr from
    // QJsonTreeItem::load()). Without this loop the new nodes leave
    // mParent dangling, and QJsonModel::parent() crashes on the next
    // paint (parentItem->row() dereferences null).
    for (QJsonTreeItem *c : mChilds)
    {
        if (c) c->setParent(this);
    }
}


void QJsonTreeItem::appendChild(QJsonTreeItem *item)
{
    item->setParent(this);
    mChilds.append(item);
}

void QJsonTreeItem::insertChild(int row, QJsonTreeItem *item)
{
    if (!item) return;
    if (row < 0) row = 0;
    if (row > mChilds.size()) row = mChilds.size();
    item->setParent(this);
    mChilds.insert(row, item);
}

QJsonTreeItem *QJsonTreeItem::takeChildAt(int row)
{
    if (row < 0 || row >= mChilds.size())
        return nullptr;
    return mChilds.takeAt(row);
}

QJsonTreeItem *QJsonTreeItem::child(int row)
{
    return mChilds.value(row);
}

QJsonTreeItem *QJsonTreeItem::parent()
{
    return mParent;
}

void QJsonTreeItem::setParent(QJsonTreeItem *parent)
{
    mParent = parent;
}

int QJsonTreeItem::childCount() const
{
    return mChilds.count();
}

int QJsonTreeItem::row() const
{
    if (!mParent)
        return 0;
    // Fast path: cache is fresh when parent still holds `this` at
    // the remembered slot. Any insert/remove/reorder above us
    // invalidates the slot check and we fall through to a one-time
    // indexOf, refreshing the cache for subsequent reads.
    if (mCachedRow >= 0
        && mCachedRow < mParent->mChilds.size()
        && mParent->mChilds.at(mCachedRow) == this)
        return mCachedRow;
    mCachedRow = mParent->mChilds.indexOf(const_cast<QJsonTreeItem*>(this));
    return mCachedRow;
}

void QJsonTreeItem::setKey(const QString &key)
{
    mKey = key;
}

void QJsonTreeItem::setValue(const QString &value)
{
    mValue = value;
}

void QJsonTreeItem::setToolTip(const QString &toolTip)
{
    mToolTip = toolTip;
}

void QJsonTreeItem::setType(const QJsonValue::Type &type)
{
    mType = type;
}
/* Posibility to set/remember index from another model
 * to relate them and perform selection in diff view
 */
void QJsonTreeItem::setIdxRelation(QModelIndex idxPointer)
{
    mIdxRelation = idxPointer;
}

/* set color of an item
 */
void QJsonTreeItem::setColorType(DiffColorType colorType)
{
    mColorType = colorType;
}

/* Get index of relation from another model
 * so you can select again it's for diff purposes
 */
QModelIndex QJsonTreeItem::idxRelation()
{
    return mIdxRelation;
}

QString QJsonTreeItem::key() const
{
    return mKey;
}

QString QJsonTreeItem::value() const
{
    return mValue;
}

QString QJsonTreeItem::toolTip() const
{
    return mToolTip;
}

/* get current color of item
 */
QColor QJsonTreeItem::color() const
{
    return PREF_INST->diffColor(mColorType);
}

QJsonValue::Type QJsonTreeItem::type() const
{
    return mType;
}

QString QJsonTreeItem::text() const
{
    if(mType==QJsonValue::Array)
    {
        return mKey+"\t"+typeName()+"["+QString::number(childCount())+"]\t"+mValue;
    }
    else
    {
        return mKey+"\t"+typeName()+"\t"+mValue;
    }
}

/* Get string type name of json item
 */
QString QJsonTreeItem::typeName() const
{
    //return QVariant::typeToName(mType);
    if (mType==QJsonValue::Array)
    {
        return QString("Array");
    }
    else if (mType==QJsonValue::String)
    {
        return QString("String");
    }
    else if (mType==QJsonValue::Bool)
    {
        return QString("Bool");
    }
    else if (mType==QJsonValue::Double)
    {
        return QString("Double");
    }
    else if (mType==QJsonValue::Null)
    {
        return QString("Null");
    }
    else if (mType==QJsonValue::Object)
    {
        return QString("Object");
    }
    else if (mType==QJsonValue::Undefined)
    {
        return QString("Undefined");
    }
   qDebug()<<"Fatal error wrong json type";
   return QString("Fatal");
}

QJsonValue::Type QJsonTreeItem::stringToType(const QString& typeName)
{
    if (typeName == "String") return QJsonValue::String;
    if (typeName == "Double") return QJsonValue::Double;
    if (typeName == "Bool") return QJsonValue::Bool;
    if (typeName == "Array") return QJsonValue::Array;
    if (typeName == "Object") return QJsonValue::Object;
    if (typeName == "Null") return QJsonValue::Null;
    return QJsonValue::Undefined;
}


QJsonTreeItem* QJsonTreeItem::load(const QJsonValue& value, QJsonTreeItem* parent)
{
    QJsonTreeItem * rootItem = new QJsonTreeItem(parent);
    rootItem->setKey("root");

    if ( value.isObject())
    {

        //Get all QJsonValue childs
        for (const QString& key : value.toObject().keys()){
            QJsonValue v = value.toObject().value(key);
            QJsonTreeItem * child = load(v,rootItem);
            child->setKey(key);
            child->setType(v.type());
            rootItem->appendChild(child);

        }

    }

    else if ( value.isArray())
    {
        //Get all QJsonValue childs
        int index = 0;
        for (const QJsonValue &v : value.toArray()){

            QJsonTreeItem * child = load(v,rootItem);
            child->setKey(QString::number(index));
            child->setType(v.type());
            rootItem->appendChild(child);
            ++index;
        }
    }
    else
    {
        rootItem->setValue(value.toVariant().toString());
        rootItem->setType(value.type());
    }

    return rootItem;
}

