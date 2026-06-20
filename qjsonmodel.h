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
#include <QStringList>

QT_BEGIN_NAMESPACE
class QMimeData;
QT_END_NAMESPACE

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

    // Replace the item at `target` with `source` in place — type, value,
    // and children are overwritten. The target's *key* is preserved
    // (the key identifies the item's position in its parent).
    // Used by the diff-edit "copy from peer" flow. Emits proper
    // begin/endRemoveRows + begin/endInsertRows + dataChanged so views
    // keep their selection and scroll state — no model reset.
    // Returns false if `target` is invalid or `source` is undefined.
    bool replaceFromJson(const QModelIndex &target, const QJsonValue &source);

    // Append a new child to `parent` built from `source`. For object
    // parents the new child carries `key`; for array parents the new
    // child's key is forced to its index-in-parent (matches the
    // loadJson() convention). `parent` may be invalid to append at the
    // root level (only valid when the root is Object/Array). Emits
    // begin/endInsertRows + dataChanged on the parent (its childCount
    // display changes) + modelChanged + dataUpdated.
    // Returns the QModelIndex (column 0) of the new child, or invalid.
    QModelIndex appendChildFromJson(const QModelIndex &parent,
                                    const QString &key,
                                    const QJsonValue &source);

    // Remove the row at `target`. Rejects the root. Emits
    // begin/endRemoveRows + modelChanged + dataUpdated.
    bool removeRowAt(const QModelIndex &target);

    // Insert a new child at row `row` of `parent`. Sibling of
    // appendChildFromJson — same key/value semantics, but lets the
    // caller pick the insertion position. Used by dropMimeData() so
    // mid-array drops land at the indicated row. `row` is clamped to
    // [0, childCount]; `row == childCount` is the append case.
    QModelIndex insertChildFromJson(const QModelIndex &parent,
                                    int row,
                                    const QString &key,
                                    const QJsonValue &source);

    // --- Drag-and-drop ------------------------------------------------
    // Item-level DnD between two QJsonContainer trees (e.g. the two
    // sides of a QJsonDiff). Carries a custom MIME type, copy-action
    // only. Gated by mIsEditable so the read-only contract is
    // preserved for non-editable usages — flags() omits the drag/drop
    // bits and dropMimeData/canDropMimeData refuse. File-URL drops
    // (the existing groupbox file-drop) are NOT in mimeTypes() so the
    // tree's default dragEnter handler ignores them and they
    // propagate up to the surrounding groupbox unchanged.
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList     mimeTypes() const override;
    QMimeData      *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action,
                         int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column,
                      const QModelIndex &parent) override;

    static const QString kItemMimeType;

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
