/* Author: Yuriy Kuzin
 * Description: pure compare logic for two JSON trees + a Q_OBJECT
 *   wrapper for running it on a worker QThread.
 *   No QWidget deps — only QtCore + QtGui (for the DiffColorType enum
 *   that lives next to QColor in preferences.h).
 *   Phase 1: static compare() ran synchronously on the caller.
 *   Phase 2 (this file): compareAsync() slot runs on the engine's
 *     owning thread, reports progress, can be cancelled.
 */
#ifndef JSONDIFFENGINE_H
#define JSONDIFFENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonValue>
#include <QSharedPointer>

#include <atomic>

#include "preferences/preferences.h"  // DiffColorType

class QJsonModel;

/* Plain-data snapshot of one side of the compare.
 * Built from a QJsonModel on the main thread once, then read freely.
 * The color / relationPath fields are outputs filled by JsonDiffEngine.
 */
struct DiffNode
{
    // Inputs (filled by snapshot()):
    QString             key;
    QJsonValue::Type    type = QJsonValue::Null;
    QString             value;
    QList<DiffNode>     children;

    // Outputs (filled by compare()):
    DiffColorType       color = DiffColorType::None;
    QList<int>          relationPath;  // empty == no cross-link
};

Q_DECLARE_METATYPE(DiffNode)
Q_DECLARE_METATYPE(QSharedPointer<DiffNode>)

class JsonDiffEngine : public QObject
{
    Q_OBJECT
public:
    enum class Mode
    {
        FullPath,           // default, fast: match by absolute key+type path
        ParentChildPair     // slow: first parent.key+key+type match wins
    };
    Q_ENUM(Mode)

    // Phase markers for progress reporting. Engine emits the enum;
    // the host translates it for the user. Engine itself stays free
    // of UI/translation concerns.
    enum class Phase
    {
        BuildingPaths,      // FullPath: walk both trees, collect paths
        MatchingPaths,      // FullPath: cross-reference paths via hash
        ComparingValues,    // FullPath: per-pair value/childCount compare
        IndexingRightTree,  // ParentChildPair: build hash of right tree
        PairingItems,       // ParentChildPair: walk left, find peers
        ResolvingColors    // both modes: ancestor Moderate promotion
    };
    Q_ENUM(Phase)

    explicit JsonDiffEngine(QObject *parent = nullptr);
    ~JsonDiffEngine() override;

    // --- Main-thread static helpers (read/write the model) -----------

    // Build a snapshot of a model. Main thread only.
    static DiffNode snapshot(QJsonModel *model);

    // Build a DiffNode for one model node (recursively). Used by the
    // rowsInserted hook in QJsonDiff to splice a newly-dropped subtree
    // into an existing snapshot without re-walking the whole tree.
    // Resulting node has no color/relationPath; caller is expected
    // to mark it NotPresent (or otherwise) before apply().
    static DiffNode snapshotIndex(QJsonModel *model, const QModelIndex &idx);

    // Push snapshot.color + snapshot.relationPath back onto the model's
    // QJsonTreeItems. Main thread only. Emits layoutChanged() so views
    // repaint.
    static void apply(const DiffNode &snap, QJsonModel *model, QJsonModel *otherModel);

    // --- Synchronous compare (pure compute) --------------------------

    // Fills color + relationPath in place. Used by tests, used as the
    // body of compareAsync(). No thread or signal interaction.
    static void compare(DiffNode &left, DiffNode &right, Mode mode);

    // Convert a snapshot subtree to QJsonValue. Used to hand a peer's
    // content to QJsonModel::replaceFromJson() without dragging a
    // DiffNode dependency into qjsonmodel.h.
    static QJsonValue toJsonValue(const DiffNode &node);

    // --- Phase A: targeted edit-and-recompare ------------------------

    // Copy type/value/children from `source` into `target` in place.
    // Preserves `target.key` (the key identifies the node's position
    // in its parent) and does NOT touch color/relationPath — the
    // caller is expected to follow up with recomparePair() to refresh
    // colors on the affected pair and their ancestors.
    static void copyPeer(DiffNode &target, const DiffNode &source);

    // After an edit (or a copyPeer), update the color of the matched
    // pair at (leftRoot[leftPath], rightRoot[rightPath]) and walk both
    // ancestor chains refreshing Moderate / Identical state. Cheap —
    // no full traversal of either tree. Mode is currently unused but
    // kept for forward-compatibility with mode-specific edge cases.
    static void recomparePair(DiffNode &leftRoot,  const QList<int> &leftPath,
                              DiffNode &rightRoot, const QList<int> &rightPath,
                              Mode mode);

    // --- Phase B: append / remove for NotPresent rows ----------------

    // Append a copy of `srcRoot[srcPath]` (a NotPresent node) as the
    // last child of `dstRoot[dstParentPath]`. The inserted subtree is
    // cross-linked with the source recursively so apply() will set up
    // idxRelation across every newly-mirrored node — not just the new
    // root. Refreshes ancestor colors on both sides.
    //
    // Append-only (no positional insert): keeps the implementation
    // simple by avoiding a sibling-relationPath shift. The caller is
    // expected to mirror this by APPENDING on the model side too.
    //
    // Returns the new child's structural path inside dstRoot on success
    // (so the widget can drive QJsonModel::appendChildFromJson with a
    // matching position), or an empty list on failure.
    static QList<int> insertPeer(DiffNode &srcRoot,
                                 const QList<int> &srcPath,
                                 DiffNode &dstRoot,
                                 const QList<int> &dstParentPath,
                                 Mode mode);

    // Remove the node at `srcRoot[srcPath]`. If that node was paired
    // (relationPath non-empty), the peer in `otherRoot` is set to
    // NotPresent and its relationPath cleared — orphaned, the user's
    // next move is to either push it back or delete it on that side.
    // Ancestor colors refresh on both sides. Rejects an empty path
    // (root removal).
    static bool removePeer(DiffNode &srcRoot, const QList<int> &srcPath,
                           DiffNode &otherRoot, Mode mode);

    // Detach a paired leaf from its peer: mark both sides NotPresent +
    // clear cross-links + refresh ancestor colors. Used when a user
    // renames the key of a paired item — the same slot now names a
    // different item, so the prior pairing is invalid until the next
    // Compare. Safe to call with an unpaired node (no-op on the peer
    // side). Rejects an empty path.
    static bool detachPair(DiffNode &myRoot, const QList<int> &myPath,
                           DiffNode &peerRoot, Mode mode);

    // Re-walk the model's subtree at `myPath` and replace the snapshot
    // subtree's children. The node at myPath KEEPS its key/type/value/
    // color/relationPath — only children are replaced. New children
    // default to NotPresent (no peers on the other side yet). Any
    // node in peerRoot whose relationPath pointed strictly INTO the
    // old subtree (i.e. relationPath.size() > myPath.size() and starts
    // with myPath) gets orphaned to NotPresent.
    //
    // Used by the inline-edit slot when a column-1 (type) edit
    // restructures the model's children through Object↔Array
    // conversion paths — keeps the snapshot consistent without a
    // full re-Compare. Rejects an empty path (the root has no parent
    // index to navigate to).
    static bool resnapshotSubtree(DiffNode &myRoot,
                                  const QList<int> &myPath,
                                  QJsonModel *myModel,
                                  DiffNode &peerRoot);

    // Given a NotPresent node at srcRoot[srcPath], compute the
    // destination-side parent's structural path in the other tree.
    // Returns true on success (parent has a peer, or src parent is the
    // root and we mirror to the other root). The empty parent path =
    // the other tree's root. Returns false when the source parent is
    // also NotPresent — the caller should hide the push action in that
    // case.
    static bool resolveDstParentPath(const DiffNode &srcRoot,
                                     const QList<int> &srcPath,
                                     QList<int> &dstParentPath);

    // --- Async cancellation -----------------------------------------

    // Thread-safe: callable from any thread. Asks the currently-running
    // compareAsync() to abort. No-op if no compare is running.
    void requestCancel();
    bool cancelRequested() const;

    // Thread-safe: clears the cancel flag. Callers must invoke this
    // before each compareAsync() to ensure a stale cancellation from a
    // previous run doesn't immediately abort the new one.
    void resetCancel();

public slots:
    // Runs on whichever thread this engine lives on (intended use: a
    // worker QThread). Emits progressed() during work, then exactly
    // one of finished(left, right) or cancelled().
    //
    // Payload is QSharedPointer so the DiffNode trees themselves do
    // NOT get copied across the queued-connection boundary — only the
    // shared-pointer handle crosses (atomic refcount increment). The
    // slot computes on *left / *right in place; the same pointers come
    // back out via finished(). Saves ~6 deep copies of the tree per
    // compare on the hot path.
    void compareAsync(QSharedPointer<DiffNode> left,
                      QSharedPointer<DiffNode> right,
                      JsonDiffEngine::Mode mode);

signals:
    void progressed(int done, int total);
    // Emitted at each algorithm-phase boundary so the host can update
    // the dialog label. Receiver maps the enum to a tr()'d user-facing
    // string — engine stays UI-/translation-agnostic. lupdate sees the
    // tr() calls on the host side.
    void phaseChanged(JsonDiffEngine::Phase phase);
    void finished(QSharedPointer<DiffNode> left, QSharedPointer<DiffNode> right);
    void cancelled();

private:
    std::atomic<bool> mCancelRequested{false};
};

#endif // JSONDIFFENGINE_H
