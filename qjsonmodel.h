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
    // Root item accessor. Same object mRootItem points to internally.
    // Needed by JsonDiffEngine::apply so it can splice phantom rows
    // under the root when arrayAlignment produces top-level phantoms.
    QJsonTreeItem* rootItem() const { return mRootItem; }
    void setIcon(const QJsonValue::Type& type, const QIcon& icon);
    bool hasParseError() const { return mHasParseError; }
    QString lastErrorMessage() const { return mLastErrorMessage; }
    int lastErrorOffset() const { return mLastErrorOffset; }
    QJsonDocument getJsonDocument() const;
    QJsonValue::Type rootType() const;

    void setEditable(bool editable);

    // Replace the item at `target` with `source` in place - type, value,
    // and children are overwritten. The target's *key* is preserved
    // (the key identifies the item's position in its parent).
    // Used by the diff-edit "copy from peer" flow. Emits proper
    // begin/endRemoveRows + begin/endInsertRows + dataChanged so views
    // keep their selection and scroll state - no model reset.
    // Returns false if `target` is invalid or `source` is undefined.
    bool replaceFromJson(const QModelIndex &target, const QJsonValue &source);

    // Append a new child to `parent` built from `source`. For object
    // parents the new child carries `key`; for array parents the new
    // child's key is forced to its index-in-parent (matches the
    // loadJson() convention). `parent` may be invalid to append at the
    // root level (only valid when the root is Object/Array). Emits
    // begin/endInsertRows + dataChanged on the parent (its childCount
    // display changes) + modelChanged + dataUpdated.
    // For Object parents, an empty `key` is rejected (returns invalid)
    // because QJsonObject::insert("", v) silently overwrites prior
    // empty-key siblings on serialize. For Array parents, `key` is
    // ignored - the new child's key is set to its row index.
    // Returns the QModelIndex (column 0) of the new child, or invalid.
    QModelIndex appendChildFromJson(const QModelIndex &parent,
                                    const QString &key,
                                    const QJsonValue &source);

    // Remove the row at `target`. Rejects the root. Emits
    // begin/endRemoveRows + modelChanged + dataUpdated.
    bool removeRowAt(const QModelIndex &target);

    // Insert a new child at row `row` of `parent`. Sibling of
    // appendChildFromJson - same key/value semantics, but lets the
    // caller pick the insertion position. Used by dropMimeData() so
    // mid-array drops land at the indicated row. `row` is clamped to
    // [0, childCount]; `row == childCount` is the append case.
    QModelIndex insertChildFromJson(const QModelIndex &parent,
                                    int row,
                                    const QString &key,
                                    const QJsonValue &source);

    // Insert a phantom (placeholder) row under `parent` at `row`.
    // Used by JsonDiffEngine::apply to line up matched items on the
    // two sides after an array/object alignment. Emits proper
    // begin/endInsertRows so QTreeView's persistent-index-based state
    // (expansion, selection, scroll anchor) is preserved for the rows
    // at and after the insertion point. Returns the new item, or
    // nullptr if `parent` doesn't resolve.
    QJsonTreeItem *insertPhantomRow(const QModelIndex &parent, int row);

    // --- Drag-and-drop ------------------------------------------------
    // Item-level DnD between two QJsonContainer trees (e.g. the two
    // sides of a QJsonDiff). Carries a custom MIME type, copy-action
    // only. Gated by mIsEditable so the read-only contract is
    // preserved for non-editable usages - flags() omits the drag/drop
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

    // Diff-navigation index list. JsonDiffEngine::apply resets this
    // at the top of its run and appends every index that qualifies
    // as a "reportable diff row" (non-phantom scalar with color !=
    // None/Identical) as applyRecursive walks the DiffNode tree.
    // QJsonContainer hydrates its gotoIndexes_list from here on
    // dataUpdated, so the container no longer needs the full-tree
    // fillGotoList walk after every Compare.
    void resetDiffIndices();
    void appendDiffIndex(const QModelIndex &idx);
    const QList<QModelIndex> &diffIndices() const { return mDiffIndices; }

private:
    QJsonTreeItem * mRootItem;
    QStringList mHeaders;
    QHash<QJsonValue::Type, QIcon> mTypeIcons;
    bool mHasParseError = false;
    QString mLastErrorMessage;
    int mLastErrorOffset = 0;
    bool mIsEditable = false;
    QList<QModelIndex> mDiffIndices;

    void setParseError(bool hasError, const QString& msg, int offset);
    void generateJson(QJsonTreeItem *item, QJsonValue &value) const;


signals:
   void dataUpdated();
   void parseErrorChanged();
   void modelChanged();

   // Progress hooks for JsonDiffEngine::apply. On huge JSONs the
   // per-item colour/idxRelation walk + phantom-row inserts can
   // take seconds; the container hooks a thin bar under the tree
   // view onto these so the UI stays responsive (apply yields to
   // the event loop between chunks) and the user sees progress.
   // `phantoms` = number of alignment-placeholder rows inserted so
   // far, exposed so the status label under the tree can tell the
   // user *why* the walk is spending time (colouring vs. filling
   // gaps for missing peer rows).
   void applyStarted(int totalNodes);
   void applyProgress(int done, int phantoms);
   void applyFinished();
   // Sub-phase timing for the just-completed apply(). Fired after
   // applyFinished + dataUpdated so it carries every emit-side cost.
   // Four ints (all milliseconds) keeps MOC happy without needing
   // Q_DECLARE_METATYPE on a struct.
   //   walkMs        - applyRecursive body, yield time excluded
   //   yieldsMs      - cumulative processEvents drain time
   //   finishedMs    - applyFinished slot fan-out (bar hide, viewport update, etc.)
   //   dataUpdatedMs - dataUpdated slot fan-out (on_model_dataUpdated + anyone else)
   void applyTimings(int walkMs, int yieldsMs, int finishedMs, int dataUpdatedMs);


};

#endif // QJSONMODEL_H
