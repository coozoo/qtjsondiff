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
#include <QIcon>
#include <QFont>

QJsonModel::QJsonModel(QObject *parent) :
    QAbstractItemModel(parent)
{
    mRootItem = new QJsonTreeItem;
    mHeaders.append("Name");
    mHeaders.append("Type");
    mHeaders.append("Value");


}

QJsonModel::~QJsonModel()
{
    delete mRootItem;
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
        QJsonDocument mDocument=QJsonDocument::fromJson(json,&parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
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
        if(!mDocument.isNull())
        {
            beginResetModel();
            delete mRootItem;
            if(mDocument.isObject())
            {
                mRootItem = QJsonTreeItem::load(QJsonValue(mDocument.object()));
                mRootItem->setType(QJsonValue::Object);
            }
            else
            {
                mRootItem = QJsonTreeItem::load(QJsonValue(mDocument.array()));
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
    emit dataUpdated();
    return false;
}


QVariant QJsonModel::data(const QModelIndex &index, int role) const
{

    if (!index.isValid())
        return QVariant();


    QJsonTreeItem *item = static_cast<QJsonTreeItem*>(index.internalPointer());

    if (role == Qt::BackgroundColorRole)
    {
        if(!item->color().isValid())
        {
            return QColor(Qt::white);
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

    if (role == Qt::DisplayRole) {

        if (index.column() == 0)
            return QString("%1").arg(item->key());
        if (index.column() == 1)
        {
            if (item->type()==QJsonValue::Array)
            {
                return QString("%1").arg(QString("Array") + QString("[") + QString::number(item->childCount()) + QString("]"));
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
    return 3;
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
    for(int k = 0; k < (parents.size()/2); k++) parents.swap(k,parents.size()-(1+k));
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

