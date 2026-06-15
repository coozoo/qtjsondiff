/* Author: Yuriy Kuzin
 * Description: implementation of JsonDiffEngine.
 *   Ports the original QJsonDiff comparison algorithm onto DiffNode
 *   trees. Behaviour is preserved exactly — including a couple of
 *   harmless-by-accident expressions in the original — so the existing
 *   widget tests stay green through this refactor.
 *
 *   Phase 3 polish:
 *     - per-node progress tracking with 1%-change throttled emit
 *       (so the dialog moves smoothly through huge inputs)
 *     - cancel check rolled into the same tick (per-node, not per-phase)
 *     - QHash<QString,int> lookup in FullPath matching replaces the
 *       original O(N²) QStringList::indexOf — large compares are now
 *       linear in the tree size
 */
#include "jsondiffengine.h"

#include "qjsonmodel.h"
#include "qjsonitem.h"

#include <QModelIndex>
#include <QHash>
#include <functional>

namespace
{

// -----------------------------------------------------------------------
// ProgressTracker — single object threaded through the algorithm.
// tick() increments a work counter, returns false if cancel was set.
// Emits the on-progress callback only when the integer percentage
// changes (so at most ~100 emits per compare regardless of work size).
// -----------------------------------------------------------------------

class ProgressTracker
{
public:
    ProgressTracker(int total,
                    std::atomic<bool> *cancel,
                    std::function<void(int, int)> onProgress)
        : mTotal(total), mCancel(cancel), mOnProgress(std::move(onProgress))
    {}

    bool tick()
    {
        if (mCancel && mCancel->load(std::memory_order_relaxed))
            return false;
        ++mDone;
        if (mOnProgress && mTotal > 0)
        {
            const int pct = (mDone * 100) / mTotal;
            if (pct != mLastPct)
            {
                mLastPct = pct;
                mOnProgress(mDone, mTotal);
            }
        }
        return true;
    }

    bool isCancelled() const
    {
        return mCancel && mCancel->load(std::memory_order_relaxed);
    }

    void emitFinal()
    {
        if (mOnProgress && mTotal > 0)
            mOnProgress(mTotal, mTotal);
    }

private:
    int mTotal;
    int mDone = 0;
    int mLastPct = -1;
    std::atomic<bool> *mCancel;
    std::function<void(int, int)> mOnProgress;
};

// Count of nodes reachable as children-recursively from `n` (root itself
// not included). Used to estimate total work for progress reporting.
int treeSize(const DiffNode &n)
{
    int total = 0;
    for (const auto &c : n.children)
        total += 1 + treeSize(c);
    return total;
}

// -----------------------------------------------------------------------
// Snapshot builder helpers (main thread, no tracker)
// -----------------------------------------------------------------------

void snapshotChildren(QJsonModel *model, const QModelIndex &parent, DiffNode &dest)
{
    const int n = model->rowCount(parent);
    dest.children.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        QModelIndex idx = model->index(i, 0, parent);
        QJsonTreeItem *item = model->itemFromIndex(idx);
        DiffNode child;
        child.key   = item->key();
        child.type  = item->type();
        child.value = item->value();
        snapshotChildren(model, idx, child);
        dest.children.append(child);
    }
}

// -----------------------------------------------------------------------
// Path utilities — preserve the original QJsonModel::jsonPath() format
// so full-path matching is bit-for-bit identical to the old algorithm.
// -----------------------------------------------------------------------

QString typeName(QJsonValue::Type t)
{
    switch (t)
    {
        case QJsonValue::Array:     return "Array";
        case QJsonValue::String:    return "String";
        case QJsonValue::Bool:      return "Bool";
        case QJsonValue::Double:    return "Double";
        case QJsonValue::Null:      return "Null";
        case QJsonValue::Object:    return "Object";
        case QJsonValue::Undefined: return "Undefined";
    }
    return "Fatal";
}

// Recursively walk a snapshot, producing:
//  - paths:    same path-string format as the original QJsonModel::jsonPath()
//              ("root(Object)->k(Double)"), so contains() lookups match
//  - nodes:    parallel list of DiffNode pointers
//  - idxPaths: parallel list of QList<int> structural paths (used to
//              cross-link via relationPath in the result)
// Returns false if cancellation was requested mid-walk.
bool buildPathList(DiffNode &node,
                   const QString &parentPath,
                   const QList<int> &parentIdxPath,
                   QStringList &paths,
                   QList<DiffNode*> &nodes,
                   QList<QList<int>> &idxPaths,
                   ProgressTracker *tracker)
{
    for (int i = 0; i < node.children.size(); ++i)
    {
        if (tracker && !tracker->tick())
            return false;
        DiffNode &child = node.children[i];
        QString childPath = parentPath + "->" + child.key + "(" + typeName(child.type) + ")";
        QList<int> childIdxPath = parentIdxPath;
        childIdxPath.append(i);

        paths.append(childPath);
        nodes.append(&child);
        idxPaths.append(childIdxPath);

        if (!buildPathList(child, childPath, childIdxPath, paths, nodes, idxPaths, tracker))
            return false;
    }
    return true;
}

// First-occurrence index for each path string. Preserves the original
// indexOf() semantic where the first-listed path wins on duplicates
// (since QHash::insert would overwrite, we check contains() first).
QHash<QString, int> buildPathIndex(const QStringList &paths)
{
    QHash<QString, int> idx;
    idx.reserve(paths.size());
    for (int i = 0; i < paths.size(); ++i)
    {
        if (!idx.contains(paths[i]))
            idx.insert(paths[i], i);
    }
    return idx;
}

// -----------------------------------------------------------------------
// FullPath mode
// -----------------------------------------------------------------------

bool comparePathOneWay(QList<DiffNode*> &myNodes,
                       const QStringList &myPaths,
                       const QList<QList<int>> &myIdxPaths,
                       const QList<DiffNode*> &otherNodes,
                       const QHash<QString, int> &otherIdx,
                       const QList<QList<int>> &otherIdxPaths,
                       ProgressTracker *tracker)
{
    for (int i = 0; i < myPaths.size(); ++i)
    {
        if (tracker && !tracker->tick())
            return false;
        DiffNode *node = myNodes[i];
        if (node->color != DiffColorType::Identical)
        {
            const int j = otherIdx.value(myPaths[i], -1);
            if (j >= 0)
            {
                node->color = DiffColorType::Identical;
                node->relationPath = otherIdxPaths[j];
                otherNodes[j]->color = DiffColorType::Identical;
                otherNodes[j]->relationPath = myIdxPaths[i];
            }
            else
            {
                node->color = DiffColorType::NotPresent;
            }
        }
    }
    return true;
}

bool compareValueOneWay(QList<DiffNode*> &myNodes,
                        DiffNode &otherRoot,
                        ProgressTracker *tracker)
{
    for (int i = 0; i < myNodes.size(); ++i)
    {
        if (tracker && !tracker->tick())
            return false;
        DiffNode *node = myNodes[i];
        if (node->color != DiffColorType::Identical || node->relationPath.isEmpty())
            continue;

        // Walk other tree by relationPath to find the matched node.
        DiffNode *peer = &otherRoot;
        bool ok = true;
        for (int step : node->relationPath)
        {
            if (step < 0 || step >= peer->children.size()) { ok = false; break; }
            peer = &peer->children[step];
        }
        if (!ok) continue;

        // Preserve the original (accidentally-OR'd) expression: it
        // collapses to (value mismatch) for scalars and to (childCount
        // mismatch) for containers because containers carry empty value
        // strings.
        if ((node->type != QJsonValue::Array || node->type != QJsonValue::Object)
                && node->value != peer->value)
        {
            node->color = DiffColorType::Huge;
            peer->color = DiffColorType::Huge;
        }
        else
        {
            if (node->children.size() != peer->children.size())
            {
                node->color = DiffColorType::Huge;
                peer->color = DiffColorType::Huge;
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------
// ParentChild mode
// -----------------------------------------------------------------------

// Search the entire right tree for the first node that matches itemLeft's
// (parent.key, key, type). Same semantics as the original findIndexInModel.
// Does NOT call tracker->tick() — that happens once per left node at the
// compareModelsRecursive level — but does check isCancelled() per iter.
int findInRight(DiffNode &itemLeft,
                const QList<int> &leftPath,
                const QString &leftParentKey,
                DiffNode &right,
                const QList<int> &rightPath,
                ProgressTracker *tracker)
{
    for (int i = 0; i < right.children.size(); ++i)
    {
        if (tracker && tracker->isCancelled())
            return -1;
        DiffNode &candidate = right.children[i];
        QList<int> candidatePath = rightPath;
        candidatePath.append(i);

        DiffColorType leftColor  = DiffColorType::Huge;
        DiffColorType rightColor = DiffColorType::Huge;

        if (itemLeft.type == candidate.type)
        {
            if (itemLeft.color == DiffColorType::None
                    && candidate.color == DiffColorType::None
                    && leftParentKey == right.key
                    && itemLeft.key == candidate.key)
            {
                bool matched = true;
                if (itemLeft.type == QJsonValue::Array || itemLeft.type == QJsonValue::Object)
                {
                    if (itemLeft.children.size() == candidate.children.size())
                    {
                        leftColor  = DiffColorType::Identical;
                        rightColor = DiffColorType::Identical;
                    }
                }
                else if (itemLeft.type == QJsonValue::Double
                         || itemLeft.type == QJsonValue::String
                         || itemLeft.type == QJsonValue::Bool)
                {
                    if (itemLeft.value == candidate.value)
                    {
                        leftColor  = DiffColorType::Identical;
                        rightColor = DiffColorType::Identical;
                    }
                }
                else if (itemLeft.type == QJsonValue::Null)
                {
                    if (itemLeft.key == candidate.key)
                    {
                        leftColor  = DiffColorType::Identical;
                        rightColor = DiffColorType::Identical;
                    }
                }
                else
                {
                    matched = false;
                }

                if (matched)
                {
                    candidate.color = rightColor;
                    itemLeft.color  = leftColor;
                    candidate.relationPath = leftPath;
                    itemLeft.relationPath  = candidatePath;
                    return 0;
                }
            }
        }
        else
        {
            // Second-stage match: types differ but parent.key + key match.
            if (itemLeft.color == DiffColorType::None
                    && candidate.color == DiffColorType::None
                    && leftParentKey == right.key
                    && itemLeft.key == candidate.key)
            {
                candidate.color = DiffColorType::Huge;
                itemLeft.color  = DiffColorType::Huge;
                candidate.relationPath = leftPath;
                itemLeft.relationPath  = candidatePath;
                return 0;
            }
        }

        // Recurse depth-first into the right subtree.
        int r = findInRight(itemLeft, leftPath, leftParentKey,
                            candidate, candidatePath, tracker);
        if (r == 0) return 0;
    }
    return -1;
}

bool compareModelsRecursive(DiffNode &left,
                            const QList<int> &leftPath,
                            DiffNode &rightRoot,
                            ProgressTracker *tracker)
{
    for (int i = 0; i < left.children.size(); ++i)
    {
        if (tracker && !tracker->tick())
            return false;
        DiffNode &child = left.children[i];
        QList<int> childPath = leftPath;
        childPath.append(i);

        if (child.color == DiffColorType::None)
        {
            findInRight(child, childPath, left.key, rightRoot, {}, tracker);
            if (tracker && tracker->isCancelled())
                return false;
        }
        if (!compareModelsRecursive(child, childPath, rightRoot, tracker))
            return false;
    }
    return true;
}

// -----------------------------------------------------------------------
// fixColors: promote parents of diffs to Moderate, convert untouched None
// items to NotPresent. Same as the original fixColors().
// -----------------------------------------------------------------------

bool fixColors(DiffNode &node, ProgressTracker *tracker)
{
    for (DiffNode &child : node.children)
    {
        if (tracker && !tracker->tick())
            return false;
        if (!fixColors(child, tracker))
            return false;

        if (child.color != DiffColorType::Identical
                && child.color != DiffColorType::None
                && child.color != DiffColorType::NotPresent
                && node.color != DiffColorType::Huge
                && node.color != DiffColorType::NotPresent)
        {
            node.color = DiffColorType::Moderate;
        }

        if (child.color == DiffColorType::None)
        {
            child.color = DiffColorType::NotPresent;
        }
    }
    return true;
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

DiffNode JsonDiffEngine::snapshot(QJsonModel *model)
{
    DiffNode root;
    root.key = "root";
    if (model)
    {
        root.type = model->rootType();
        snapshotChildren(model, QModelIndex(), root);
    }
    else
    {
        root.type = QJsonValue::Object;
    }
    return root;
}

namespace
{

QModelIndex resolveIndex(QJsonModel *model, const QList<int> &path)
{
    QModelIndex idx;
    for (int step : path)
    {
        idx = model->index(step, 0, idx);
        if (!idx.isValid())
            return QModelIndex();
    }
    return idx;
}

void applyRecursive(const DiffNode &snap, QJsonModel *model,
                    const QModelIndex &parent, QJsonModel *otherModel)
{
    for (int i = 0; i < snap.children.size(); ++i)
    {
        const DiffNode &snapChild = snap.children[i];
        QModelIndex idx = model->index(i, 0, parent);
        QJsonTreeItem *item = model->itemFromIndex(idx);
        if (!item)
            continue;

        item->setColorType(snapChild.color);
        if (snapChild.relationPath.isEmpty())
            item->setIdxRelation(QModelIndex());
        else
            item->setIdxRelation(resolveIndex(otherModel, snapChild.relationPath));

        applyRecursive(snapChild, model, idx, otherModel);
    }
}

} // anonymous namespace

void JsonDiffEngine::apply(const DiffNode &snap, QJsonModel *model, QJsonModel *otherModel)
{
    if (!model)
        return;
    applyRecursive(snap, model, QModelIndex(), otherModel);
    // Tell views to repaint — same approach as the original algorithm.
    emit model->layoutChanged();
}

namespace
{

// Internal compare. `tracker` may be null for the pure synchronous path.
// Returns whether the compare ran to completion (true) or was cancelled
// (false). Caller uses that to decide between finished() and cancelled().
bool compareInternal(DiffNode &left, DiffNode &right,
                     JsonDiffEngine::Mode mode,
                     ProgressTracker *tracker)
{
    if (mode == JsonDiffEngine::Mode::FullPath)
    {
        QStringList                leftPaths;
        QList<DiffNode*>           leftNodes;
        QList<QList<int>>          leftIdxPaths;
        const QString prefix = "root(" + typeName(left.type) + ")";
        if (!buildPathList(left, prefix, {}, leftPaths, leftNodes, leftIdxPaths, tracker))
            return false;

        QStringList                rightPaths;
        QList<DiffNode*>           rightNodes;
        QList<QList<int>>          rightIdxPaths;
        const QString rprefix = "root(" + typeName(right.type) + ")";
        if (!buildPathList(right, rprefix, {}, rightPaths, rightNodes, rightIdxPaths, tracker))
            return false;

        // O(N+M) hash-based lookup replaces the original O(N²) indexOf.
        const QHash<QString, int> leftIdx  = buildPathIndex(leftPaths);
        const QHash<QString, int> rightIdx = buildPathIndex(rightPaths);

        if (!comparePathOneWay(leftNodes,  leftPaths,  leftIdxPaths,
                               rightNodes, rightIdx,   rightIdxPaths, tracker))
            return false;
        if (!comparePathOneWay(rightNodes, rightPaths, rightIdxPaths,
                               leftNodes,  leftIdx,    leftIdxPaths, tracker))
            return false;

        if (!compareValueOneWay(leftNodes,  right, tracker)) return false;
        if (!compareValueOneWay(rightNodes, left,  tracker)) return false;

        if (!fixColors(left,  tracker)) return false;
        if (!fixColors(right, tracker)) return false;
    }
    else
    {
        if (!compareModelsRecursive(left, {}, right, tracker)) return false;
        if (!fixColors(left,  tracker)) return false;
        if (!fixColors(right, tracker)) return false;
    }

    if (tracker)
        tracker->emitFinal();
    return true;
}

} // anonymous namespace

JsonDiffEngine::JsonDiffEngine(QObject *parent) :
    QObject(parent)
{
    // Required for queued cross-thread signals. qRegisterMetaType is
    // idempotent so repeat calls are harmless.
    qRegisterMetaType<DiffNode>("DiffNode");
    qRegisterMetaType<QSharedPointer<DiffNode>>("QSharedPointer<DiffNode>");
}

JsonDiffEngine::~JsonDiffEngine() = default;

void JsonDiffEngine::compare(DiffNode &left, DiffNode &right, Mode mode)
{
    // Synchronous path: no tracker, no progress, no cancel.
    compareInternal(left, right, mode, nullptr);
}

void JsonDiffEngine::requestCancel()
{
    mCancelRequested.store(true, std::memory_order_relaxed);
}

bool JsonDiffEngine::cancelRequested() const
{
    return mCancelRequested.load(std::memory_order_relaxed);
}

void JsonDiffEngine::resetCancel()
{
    mCancelRequested.store(false, std::memory_order_relaxed);
}

void JsonDiffEngine::compareAsync(QSharedPointer<DiffNode> left,
                                  QSharedPointer<DiffNode> right,
                                  Mode mode)
{
    // Cancel state is NOT reset here — caller (orchestrator on the main
    // thread) controls that via resetCancel(). Pre-set cancel aborts at
    // the first tick().

    if (!left || !right)
    {
        emit cancelled();
        return;
    }

    // Estimate total work. FullPath ticks each side 4×: buildPath,
    // comparePath, compareValue, fixColors. ParentChild ticks left twice
    // (compareModels + fixColors) and right once (fixColors). These are
    // approximate — actual costs vary, especially ParentChild where
    // findInRight is unbounded — but they make the dialog move at all.
    const int leftSize  = treeSize(*left);
    const int rightSize = treeSize(*right);
    const int total = (mode == Mode::FullPath)
        ? 4 * (leftSize + rightSize)
        : (2 * leftSize + rightSize);

    ProgressTracker tracker(
        total,
        &mCancelRequested,
        [this](int done, int t) { emit progressed(done, t); }
    );

    // Compute in place on the heap-allocated DiffNodes; no tree copies.
    const bool ranToCompletion = compareInternal(*left, *right, mode, &tracker);

    if (!ranToCompletion || mCancelRequested.load(std::memory_order_relaxed))
    {
        emit cancelled();
    }
    else
    {
        // Only the shared-pointer handle crosses back; the trees stay
        // on the heap until the receiver releases the last reference.
        emit finished(left, right);
    }
}
