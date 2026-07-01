/*
 * Creator Schutz Sacha
 * Modified by Yuriy Kuzin
 * Updated to support diff
 *
 */

#ifndef JSONITEM_H
#define JSONITEM_H
#include <QtCore>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>
#include <QStyleOptionViewItem>
#include <QPainter>

#include "preferences/preferences.h"


class QJsonTreeItem
{
public:
    QJsonTreeItem(QJsonTreeItem * parent = nullptr);
    ~QJsonTreeItem();
    void appendChild(QJsonTreeItem * item);
    QJsonTreeItem *child(int row);
    QJsonTreeItem *parent();
    void setParent(QJsonTreeItem* parent);
    int childCount() const;
    int row() const;
    void setKey(const QString& key);
    void setValue(const QString& value);
    void setToolTip(const QString& toolTip);
    void setType(const QJsonValue::Type& type);
    void setColorType(DiffColorType colorType);
    void setIdxRelation(QModelIndex idxPointer);
    QString key() const;
    QString value() const;
    QJsonValue::Type type() const;
    QString typeName() const;
    QString text() const;
    QString toolTip() const;
    QColor color() const;
    DiffColorType colorType() const { return mColorType; }
    QModelIndex idxRelation();
    void clearChildren();

    QList<QJsonTreeItem*> takeChildren();
    void setChildren(const QList<QJsonTreeItem*>& children);

    // Single-child structural helpers. Used by QJsonModel's
    // begin/endInsertRows and begin/endRemoveRows paths so mChilds
    // is only ever off-by-one during the transaction (never emptied
    // then refilled), which keeps rowCount() consistent for any
    // proxy or observer that queries the model mid-transition.
    void insertChild(int row, QJsonTreeItem *item);
    QJsonTreeItem *takeChildAt(int row);
    
    static QJsonValue::Type stringToType(const QString& typeName);
    static QJsonTreeItem* load(const QJsonValue& value, QJsonTreeItem * parent = nullptr);

protected:


private:
    QString mKey;
    QString mTypeName;
    QString mValue;
    QString mToolTip;
    QJsonValue::Type mType;
    DiffColorType mColorType = DiffColorType::None;
    QModelIndex mIdxRelation;

    QList<QJsonTreeItem*> mChilds;
    QJsonTreeItem * mParent;


};

//Q_DECLARE_METATYPE(QJsonTreeItem)

#endif // JSONITEM_H
