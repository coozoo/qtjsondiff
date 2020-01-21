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
class QJsonTreeItem
{
public:
    QJsonTreeItem(QJsonTreeItem * parent = nullptr);
    ~QJsonTreeItem();
    void appendChild(QJsonTreeItem * item);
    QJsonTreeItem *child(int row);
    QJsonTreeItem *parent();
    int childCount() const;
    int row() const;
    void setKey(const QString& key);
    void setValue(const QString& value);
    void setToolTip(const QString& toolTip);
    void setType(const QJsonValue::Type& type);
    void setColor(const QColor& color);
    void setIdxRelation(QModelIndex idxPointer);
    QString key() const;
    QString value() const;
    QJsonValue::Type type() const;
    QString typeName() const;
    QString text() const;
    QString toolTip() const;
    QColor color() const;
    QModelIndex idxRelation();

    static QJsonTreeItem* load(const QJsonValue& value, QJsonTreeItem * parent = 0);

protected:


private:
    QString mKey;
    QString mTypeName;
    QString mValue;
    QString mToolTip;
    QJsonValue::Type mType;
    QColor mColor;
    QModelIndex mIdxRelation;

    QList<QJsonTreeItem*> mChilds;
    QJsonTreeItem * mParent;


};

//Q_DECLARE_METATYPE(QJsonTreeItem)

#endif // JSONITEM_H
