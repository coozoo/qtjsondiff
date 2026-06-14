/*
 * Creator Schutz Sacha
 * Modified by Yuriy Kuzin
 * Updated to support diff
 *
 */

#ifndef QJSONMODEL_H
#define QJSONMODEL_H

#include <QAbstractItemModel>
#include "qjsonitem.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QIcon>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QStyledItemDelegate>

class QJsonModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(bool hasParseError READ hasParseError NOTIFY parseErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY parseErrorChanged)
public:
    explicit QJsonModel(QObject *parent = nullptr);
    ~QJsonModel() override;
    bool load(const QString& fileName);
    bool load(QIODevice * device);
    bool loadJson(const QByteArray& json);
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column,const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QList<QModelIndex> parents(QModelIndex &index) const;
    QList<QModelIndex> jsonIndexPath(QModelIndex &index) const;
    QString jsonPath(QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QJsonTreeItem* itemFromIndex(const QModelIndex &index) const;
    void setIcon(const QJsonValue::Type& type, const QIcon& icon);
    bool hasParseError() const { return mHasParseError; }
    QString lastErrorMessage() const { return mLastErrorMessage; }
    int lastErrorOffset() const { return mLastErrorOffset; }
    QJsonDocument getJsonDocument() const;
    QJsonValue::Type rootType() const;

    void setEditable(bool editable);

private:
    QJsonTreeItem * mRootItem;
    QStringList mHeaders;
    QHash<QJsonValue::Type, QIcon> mTypeIcons;
    bool mHasParseError = false;
    QString mLastErrorMessage;
    int mLastErrorOffset = 0;
    bool mIsEditable = false;

    void setParseError(bool hasError, const QString& msg, int offset);
    void generateJson(QJsonTreeItem *item, QJsonValue &value) const;


signals:
   void dataUpdated();
   void parseErrorChanged();
   void modelChanged();


};

#endif // QJSONMODEL_H
