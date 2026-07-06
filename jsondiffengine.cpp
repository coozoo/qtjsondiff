/* Author: Yuriy Kuzin
 * Description: implementation of JsonDiffEngine.
 *   Ports the original QJsonDiff comparison algorithm onto DiffNode
 *   trees. Behavior is preserved exactly - including a couple of
 *   harmless-by-accident expressions in the original - so the existing
 *   widget tests stay green through this refactor.
 *
 *   Phase 3 polish:
 *     - per-node progress tracking with 1%-change throttled emit
 *       (so the dialog moves smoothly through huge inputs)
 *     - cancel check rolled into the same tick (per-node, not per-phase)
 *     - QHash<QString,int> lookup in FullPath matching replaces the
 *       original O(N²) QStringList::indexOf - large compares are now
 *       linear in the tree size
 */
#include "jsondiffengine.h"

#include "qjsonmodel.h"
#include "qjsonitem.h"

#include <QModelIndex>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

namespace
{

// -----------------------------------------------------------------------
// ProgressTracker - single object threaded through the algorithm.
// tick() increments a work counter, returns false if cancel was set.
// Emits the on-progress callback only when the integer percentage
// changes (so at most ~100 emits per compare regardless of work size).
// -----------------------------------------------------------------------

class ProgressTracker
{
public:
    ProgressTracker(int total,
                    std::atomic<bool> *cancel,
                    std::function<void(int, int)> onProgress,
                    std::function<void(JsonDiffEngine::Phase)> onPhase = nullptr)
        : mTotal(total), mCancel(cancel),
          mOnProgress(std::move(onProgress)),
          mOnPhase(std::move(onPhase))
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

    // Announce a new algorithm phase. The host typically rewrites the
    // dialog label. No-op if no callback was wired in.
    void setPhase(JsonDiffEngine::Phase phase)
    {
        if (mOnPhase) mOnPhase(phase);
    }

private:
    int mTotal;
    int mDone = 0;
    int mLastPct = -1;
    std::atomic<bool> *mCancel;
    std::function<void(int, int)> mOnProgress;
    std::function<void(JsonDiffEngine::Phase)> mOnPhase;
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

// Canonical, key-sorted stringification of a DiffNode's VALUE. Two
// subtrees equal-as-JSON produce equal strings (modulo key order in
// objects). Used only as input to the weight fingerprint; not intended
// as valid JSON on its own (no escape handling for embedded quotes).
QString canonicalValueString(const DiffNode &node)
{
    switch (node.type)
    {
        case QJsonValue::Object:
        {
            QList<int> order;
            order.reserve(node.children.size());
            for (int i = 0; i < node.children.size(); ++i)
                order << i;
            std::sort(order.begin(), order.end(),
                      [&](int a, int b) {
                          return node.children[a].key < node.children[b].key;
                      });
            QString out = QStringLiteral("{");
            for (int i = 0; i < order.size(); ++i)
            {
                if (i) out += QLatin1Char(',');
                const DiffNode &c = node.children[order[i]];
                out += QLatin1Char('"');
                out += c.key;
                out += QLatin1String("\":");
                out += canonicalValueString(c);
            }
            out += QLatin1Char('}');
            return out;
        }
        case QJsonValue::Array:
        {
            // Sort children by canonical form so array hash is
            // order-independent. Same content in different order gives
            // the same fingerprint - the whole point of using weight
            // as an alignment identity is content, not position.
            QStringList childs;
            childs.reserve(node.children.size());
            for (const auto &c : node.children)
                childs.append(canonicalValueString(c));
            std::sort(childs.begin(), childs.end());
            QString out = QStringLiteral("[");
            for (int i = 0; i < childs.size(); ++i)
            {
                if (i) out += QLatin1Char(',');
                out += childs[i];
            }
            out += QLatin1Char(']');
            return out;
        }
        case QJsonValue::String:
            return QLatin1Char('"') + node.value + QLatin1Char('"');
        case QJsonValue::Bool:
        case QJsonValue::Double:
        case QJsonValue::Null:
        case QJsonValue::Undefined:
        default:
            return node.value;
    }
}

// Content fingerprint used for identity matching in alignment. Must
// be collision-resistant across strings a user might feed us -
// especially permutation ids like "7571927"/"7571918" that a naive
// sum-of-chars hash conflates. qHash uses SipHash, which is fine here.
qint64 weightOfString(const QString &s)
{
    return static_cast<qint64>(qHash(s));
}

void snapshotChildren(QJsonModel *model, const QModelIndex &parent, DiffNode &dest)
{
    const int n = model->rowCount(parent);
    dest.children.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        QModelIndex idx = model->index(i, 0, parent);
        QJsonTreeItem *item = model->itemFromIndex(idx);
        // Phantoms are alignment-only placeholders inserted by apply();
        // they must not appear in the snapshot the engine reasons over.
        if (item->isPhantom())
            continue;
        DiffNode child;
        child.key   = item->key();
        child.type  = item->type();
        child.value = item->value();
        snapshotChildren(model, idx, child);
        dest.children.append(child);
    }
    dest.weight = weightOfString(canonicalValueString(dest));
}

// -----------------------------------------------------------------------
// Path utilities - preserve the original QJsonModel::jsonPath() format
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

// Pre-built index: (parent.key, child.key) → list of candidates in
// pre-order DFS, used to replace the original O(N×M) recursive walk
// over the right tree with O(K) hash lookups (K = candidates per key
// pair, typically 1).
struct ParentChildEntry
{
    DiffNode  *node;
    QList<int> path;
};

// Single QString key joins parent + '\x1F' + child. ASCII Unit
// Separator can't appear in a parsed JSON key, so no collisions.
QString indexKey(const QString &parent, const QString &child)
{
    return parent + QChar('\x1F') + child;
}

using FindInRightIndex = QHash<QString, QList<ParentChildEntry>>;

// Walks `node`'s subtree in pre-order DFS, appending each child to the
// index under (node.key, child.key). Insertion order matches the visit
// order the original recursive findInRight used, so first-occurrence
// semantics are preserved bit-for-bit.
void buildFindInRightIndex(DiffNode &node,
                           const QList<int> &path,
                           FindInRightIndex &index)
{
    for (int i = 0; i < node.children.size(); ++i)
    {
        DiffNode &child = node.children[i];
        QList<int> childPath = path;
        childPath.append(i);
        index[indexKey(node.key, child.key)]
            .append({&child, childPath});
        buildFindInRightIndex(child, childPath, index);
    }
}

// Look up the first match for itemLeft in the pre-built index. Returns
// 0 on match (and writes color + relationPath on both sides) or -1
// otherwise. The match logic mirrors the original findInRight branch
// by branch.
int findInRightHashed(DiffNode &itemLeft,
                      const QList<int> &leftPath,
                      const QString &leftParentKey,
                      const FindInRightIndex &index,
                      ProgressTracker *tracker)
{
    if (itemLeft.color != DiffColorType::None) return -1;
    auto it = index.find(indexKey(leftParentKey, itemLeft.key));
    if (it == index.end()) return -1;

    for (const ParentChildEntry &e : *it)
    {
        if (tracker && tracker->isCancelled()) return -1;
        DiffNode &candidate = *e.node;
        if (candidate.color != DiffColorType::None) continue;

        DiffColorType leftColor  = DiffColorType::Huge;
        DiffColorType rightColor = DiffColorType::Huge;

        if (itemLeft.type == candidate.type)
        {
            bool matched = true;
            if (itemLeft.type == QJsonValue::Array
                    || itemLeft.type == QJsonValue::Object)
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
                // Hash already enforces key match; Null pairs same-keyed
                // entries as Identical, exactly like the original.
                leftColor  = DiffColorType::Identical;
                rightColor = DiffColorType::Identical;
            }
            else
            {
                // Undefined / unknown types: original walks past without
                // pairing. Same here.
                matched = false;
            }

            if (matched)
            {
                candidate.color = rightColor;
                itemLeft.color  = leftColor;
                candidate.relationPath = leftPath;
                itemLeft.relationPath  = e.path;
                return 0;
            }
        }
        else
        {
            // Type-mismatch fallback: same (parent.key, key), Huge pair.
            candidate.color = DiffColorType::Huge;
            itemLeft.color  = DiffColorType::Huge;
            candidate.relationPath = leftPath;
            itemLeft.relationPath  = e.path;
            return 0;
        }
    }
    return -1;
}

bool compareModelsRecursive(DiffNode &left,
                            const QList<int> &leftPath,
                            const FindInRightIndex &rightIndex,
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
            findInRightHashed(child, childPath, left.key, rightIndex, tracker);
            if (tracker && tracker->isCancelled())
                return false;
        }
        if (!compareModelsRecursive(child, childPath, rightIndex, tracker))
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

DiffNode JsonDiffEngine::snapshotIndex(QJsonModel *model, const QModelIndex &idx)
{
    DiffNode n;
    if (!model || !idx.isValid()) return n;
    QJsonTreeItem *item = model->itemFromIndex(idx);
    if (!item) return n;
    n.key   = item->key();
    n.type  = item->type();
    n.value = item->value();
    snapshotChildren(model, idx, n);
    return n;
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
    // Container with an alignment (arrayAlignment is populated on both
    // Arrays AND Objects - name kept for source compat; semantic is
    // "children pairing"). Bipartite pairs (myIdx, peerIdx). Two apply
    // strategies:
    //   - Merge-sequence (LCS-style): when matched pairs sort in the
    //     same order on both sides, we interleave orphans and phantoms
    //     at the natural gap positions so rows on the two sides line
    //     up 1:1.
    //   - Append-at-end fallback: when matches are heavily reordered
    //     the merge-sequence would require reshuffling real items, so
    //     we keep them at their original positions and (a) if both
    //     sides have orphans, skip phantoms; (b) else append phantoms
    //     at the end for the peer's orphans.
    const bool isAlignableContainer = (snap.type == QJsonValue::Array
                                       || snap.type == QJsonValue::Object);
    if (isAlignableContainer && !snap.arrayAlignment.isEmpty())
    {
        QJsonTreeItem *parentItem = parent.isValid()
                                    ? model->itemFromIndex(parent)
                                    : model->rootItem();
        const QList<int> peerArrayPath = snap.relationPath;

        // Bucket the alignment into matched / my-side orphan / peer-side orphan.
        QList<QPair<int, int>> matched;
        QList<int> myOrphans;
        QList<int> peerOrphans;
        for (const auto &p : snap.arrayAlignment)
        {
            if (p.first != -1 && p.second != -1)      matched.append(p);
            else if (p.first  != -1)                  myOrphans.append(p.first);
            else if (p.second != -1)                  peerOrphans.append(p.second);
        }
        std::sort(matched.begin(), matched.end());
        std::sort(myOrphans.begin(), myOrphans.end());
        std::sort(peerOrphans.begin(), peerOrphans.end());

        // Peer-monotonic = matched pairs sorted by my-idx also have
        // strictly-increasing peer-idx. When true we can interleave
        // safely; when false, reshuffling real items would be needed.
        bool peerMonotonic = true;
        for (int i = 1; i < matched.size(); ++i)
        {
            if (matched[i].second <= matched[i - 1].second)
            {
                peerMonotonic = false;
                break;
            }
        }

        auto applyRealAt = [&](int row, int myIdx)
        {
            QModelIndex idx = model->index(row, 0, parent);
            QJsonTreeItem *item = model->itemFromIndex(idx);
            if (!item) return;
            const DiffNode &snapChild = snap.children[myIdx];
            item->setColorType(snapChild.color);
            if (snapChild.relationPath.isEmpty())
                item->setIdxRelation(QModelIndex());
            else
                item->setIdxRelation(resolveIndex(otherModel,
                                                  snapChild.relationPath));
            applyRecursive(snapChild, model, idx, otherModel);
        };
        auto insertPhantom = [&](int row, int peerIdx)
        {
            // Go through the model so beginInsertRows / endInsertRows
            // fire - QTreeView needs the structural signal to remap
            // its persistent-index-based expansion/selection state
            // around the shifted rows. Direct QJsonTreeItem mutation
            // would leave the view guessing and collapse everything
            // when apply() later emits its (now removed) layoutChanged.
            QJsonTreeItem *ph = model->insertPhantomRow(parent, row);
            if (!ph) return;
            if (peerIdx != -1 && otherModel && !peerArrayPath.isEmpty())
            {
                QList<int> phPeer = peerArrayPath;
                phPeer.append(peerIdx);
                ph->setIdxRelation(resolveIndex(otherModel, phPeer));
            }
        };

        if (matched.isEmpty())
        {
            // No anchors - phantoms wouldn't align anything. Just apply
            // real items at their positions; skip peer-phantom
            // insertion when both sides have orphans (avoids
            // double-ghost rows on both trees).
            for (int myIdx : myOrphans)
                applyRealAt(myIdx, myIdx);
            if (myOrphans.isEmpty())
            {
                for (int peerIdx : peerOrphans)
                    insertPhantom(parentItem->childCount(), peerIdx);
            }
        }
        else if (peerMonotonic)
        {
            // Build a merge sequence: matched pairs in my-order, with
            // my-orphans and peer-orphans slotted into their natural
            // gaps. Walking this and inserting phantoms as we go keeps
            // real items at model row = sequence index.
            int mi = 0, poi = 0, row = 0;
            for (const auto &m : matched)
            {
                while (mi < myOrphans.size() && myOrphans[mi] < m.first)
                    applyRealAt(row++, myOrphans[mi++]);
                while (poi < peerOrphans.size() && peerOrphans[poi] < m.second)
                {
                    insertPhantom(row, peerOrphans[poi++]);
                    ++row;
                }
                applyRealAt(row++, m.first);
            }
            while (mi < myOrphans.size())
                applyRealAt(row++, myOrphans[mi++]);
            while (poi < peerOrphans.size())
            {
                insertPhantom(row, peerOrphans[poi++]);
                ++row;
            }
        }
        else
        {
            // Reordered matches: keep real items at their original
            // positions, only append phantoms at end, and skip them
            // when both sides have orphans (double-ghost avoidance).
            const bool skipPhantoms = (!myOrphans.isEmpty()
                                       && !peerOrphans.isEmpty());
            for (const auto &pair : snap.arrayAlignment)
            {
                const int myIdx   = pair.first;
                const int peerIdx = pair.second;
                if (myIdx == -1)
                {
                    if (skipPhantoms) continue;
                    insertPhantom(parentItem->childCount(), peerIdx);
                    continue;
                }
                applyRealAt(myIdx, myIdx);
            }
        }
        // Repaint hint for the phantom-branch parent: color updates
        // (setColorType) don't emit dataChanged themselves; emit one
        // range signal spanning all rows so QTreeView repaints them
        // without invalidating persistent indices (i.e. without
        // collapsing expanded descendants).
        const int nRows = model->rowCount(parent);
        if (nRows > 0)
        {
            emit model->dataChanged(
                model->index(0, 0, parent),
                model->index(nRows - 1, model->columnCount() - 1, parent),
                {Qt::BackgroundRole, Qt::DecorationRole, Qt::DisplayRole});
        }
        return;
    }

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

    // Same repaint hint as the phantom-branch above. Emitted after
    // recursion so descendants have already picked up their colors,
    // but this signal covers the direct children - QTreeView repaints
    // them without a layoutChanged (which would nuke expansion state).
    if (!snap.children.empty())
    {
        const int nRows = model->rowCount(parent);
        if (nRows > 0)
        {
            emit model->dataChanged(
                model->index(0, 0, parent),
                model->index(nRows - 1, model->columnCount() - 1, parent),
                {Qt::BackgroundRole, Qt::DecorationRole, Qt::DisplayRole});
        }
    }
}

} // anonymous namespace

void JsonDiffEngine::apply(const DiffNode &snap, QJsonModel *model, QJsonModel *otherModel)
{
    if (!model)
        return;
    // No layoutChanged: it invalidates every persistent QModelIndex,
    // and QTreeView tracks expansion/selection through those. On big
    // trees the user sees every expanded node collapse after each
    // Compare - worst when Smart Array inserts phantoms. Instead
    // phantom inserts use proper begin/endInsertRows (via
    // QJsonModel::insertPhantomRow) and applyRecursive emits
    // per-parent dataChanged for the color updates.
    applyRecursive(snap, model, QModelIndex(), otherModel);
    // Counter on the peer container listens for dataUpdated to refresh
    // its "N diffs" label.
    emit model->dataUpdated();
}

void JsonDiffEngine::applyPath(const DiffNode &snap, QJsonModel *model,
                               const QList<int> &path, QJsonModel *otherModel)
{
    if (!model || path.isEmpty())
        return;

    const DiffNode *node = &snap;
    QModelIndex parent;
    const int last = model->columnCount() - 1;
    // Walk from root down to `path`. Every step is an ancestor of the
    // edited node whose color/idxRelation may have shifted (Moderate ↔
    // Identical) as a result of the edit. Emit dataChanged per cell so
    // views repaint that row without going through the layoutChanged
    // invalidation of every persistent QModelIndex.
    for (int i = 0; i < path.size(); ++i)
    {
        const int step = path[i];
        if (step < 0 || step >= node->children.size())
            return;
        node = &node->children[step];
        const QModelIndex idx = model->index(step, 0, parent);
        if (!idx.isValid())
            return;
        QJsonTreeItem *item = model->itemFromIndex(idx);
        if (item)
        {
            item->setColorType(node->color);
            item->setIdxRelation(node->relationPath.isEmpty()
                                     ? QModelIndex()
                                     : resolveIndex(otherModel,
                                                    node->relationPath));
            emit model->dataChanged(
                idx, model->index(step, last, parent),
                {Qt::BackgroundRole, Qt::DecorationRole, Qt::DisplayRole});
        }
        parent = idx;
    }
    // Counter widget listens to dataUpdated for its "N diffs" text.
    emit model->dataUpdated();
}

namespace
{

// Walk `left`, and for every Array node paired with an Array peer in
// `right` (via relationPath), fill both sides' arrayAlignment with the
// weight-LCS of their children. Left side stores (leftIdx, rightIdx);
// peer side stores the swap so each direction reads "my index first."
// Recursively force a DiffNode subtree to the given color and clear
// cross-links. Used for orphaned subtrees after alignment - no peer,
// no meaningful colors from compare's positional matching to keep.
void forceDiffNodeSubtreeColor(DiffNode &n, DiffColorType color)
{
    n.color = color;
    n.relationPath.clear();
    for (auto &c : n.children)
        forceDiffNodeSubtreeColor(c, color);
}

// Forward decls: defined in the Phase-A anonymous namespace block
// further down. Same TU, same unnamed namespace - the compiler merges
// both blocks, so declaring here and defining below is well-formed.
// The overlay post-pass reuses these navigators to walk to peers and
// re-settle ancestor colors after LCS shifts an array's own color.
DiffNode *navigateTo(DiffNode &root, const QList<int> &path);
void refreshAncestorColors(DiffNode &root, const QList<int> &leafPath);

// Forward decls for mutual recursion.
void compareContentFirst(DiffNode &lc, DiffNode &rc,
                         const QList<int> &lcPeerPath,
                         const QList<int> &rcPeerPath,
                         const QString &matchKey);
void alignObjectPair(DiffNode &lc, DiffNode &rc, const QString &matchKey);

// Align the two children lists of a paired Array/Array pair.
// Preconditions: lc.type == rc.type == Array. lc.relationPath and
// rc.relationPath are already the peer's structural path (either set
// by the caller - compareContentFirst - or preserved from the mode's
// walker under the overlay post-pass).
//
// Populates arrayAlignment on both sides, forces NotPresent on the
// orphaned side of every unmatched slot, and recurses via
// compareContentFirst into every matched pair so their subtrees compare
// against the LCS-picked peer (not whatever positional peer the outer
// walker might have chosen). Overwrites lc.color / rc.color from the
// aggregate of matched-child colors.
void alignArrayPair(DiffNode &lc, DiffNode &rc, const QString &matchKey)
{
    Q_ASSERT(lc.type == QJsonValue::Array && rc.type == QJsonValue::Array);

    const QList<int> lcPeerPath = lc.relationPath;
    const QList<int> rcPeerPath = rc.relationPath;

    QList<QPair<int, int>> align;
    if (matchKey.isEmpty())
    {
        QList<qint64> lw, rw;
        lw.reserve(lc.children.size());
        rw.reserve(rc.children.size());
        for (const auto &c : lc.children) lw.append(c.weight);
        for (const auto &c : rc.children) rw.append(c.weight);
        align = JsonDiffEngine::alignByWeight(lw, rw);
    }
    else
    {
        align = JsonDiffEngine::alignByWeightWithKey(lc.children,
                                                    rc.children,
                                                    matchKey);
    }
    lc.arrayAlignment = align;
    QList<QPair<int, int>> peerAlign;
    peerAlign.reserve(align.size());
    for (const auto &p : align)
        peerAlign.append(qMakePair(p.second, p.first));
    rc.arrayAlignment = peerAlign;

    bool anyChildDiffers = false;
    for (const auto &p : align)
    {
        if (p.first == -1 && p.second != -1)
        {
            forceDiffNodeSubtreeColor(rc.children[p.second],
                                      DiffColorType::NotPresent);
            anyChildDiffers = true;
            continue;
        }
        if (p.first != -1 && p.second == -1)
        {
            forceDiffNodeSubtreeColor(lc.children[p.first],
                                      DiffColorType::NotPresent);
            anyChildDiffers = true;
            continue;
        }
        DiffNode &lch = lc.children[p.first];
        DiffNode &rch = rc.children[p.second];
        QList<int> newLc = lcPeerPath; newLc.append(p.second);
        QList<int> newRc = rcPeerPath; newRc.append(p.first);
        compareContentFirst(lch, rch, newLc, newRc, matchKey);
        if (lch.color != DiffColorType::Identical)
            anyChildDiffers = true;
    }

    lc.color = anyChildDiffers ? DiffColorType::Moderate
                               : DiffColorType::Identical;
    rc.color = lc.color;
}

// Content-first compare. Full walk from the root down; each node's
// color derives from what its content actually is (not from any prior
// positional pairing). Objects match children by key; Arrays match
// children via alignArrayPair (bipartite / LCS). Matched pairs recurse;
// orphans get NotPresent throughout their subtree. Sets relationPath on
// every node so cross-links land on the actually-paired peer, and
// arrayAlignment on every Array so apply() knows where to append
// phantoms.
void compareContentFirst(DiffNode &lc, DiffNode &rc,
                         const QList<int> &lcPeerPath,
                         const QList<int> &rcPeerPath,
                         const QString &matchKey)
{
    lc.relationPath = lcPeerPath;
    rc.relationPath = rcPeerPath;
    lc.arrayAlignment.clear();
    rc.arrayAlignment.clear();

    if (lc.type != rc.type)
    {
        lc.color = DiffColorType::Huge;
        rc.color = DiffColorType::Huge;
        return;
    }

    const bool isContainer = (lc.type == QJsonValue::Object
                              || lc.type == QJsonValue::Array);
    if (!isContainer)
    {
        const bool same = (lc.value == rc.value);
        lc.color = same ? DiffColorType::Identical : DiffColorType::Huge;
        rc.color = lc.color;
        return;
    }

    if (lc.type == QJsonValue::Array)
        alignArrayPair(lc, rc, matchKey);
    else
        alignObjectPair(lc, rc, matchKey);
}

// Align the two children lists of a paired Object/Object pair.
// Object children are matched by key rather than by weight - key
// carries the semantic identity for objects. The output alignment
// shape is identical to alignArrayPair: (myIdx, peerIdx) with -1 on
// either side signaling orphan. Populates arrayAlignment (name kept
// for compatibility; semantic is "children pairing," object or array).
// applyRecursive reads it the same way for both container types, so
// object children with a missing peer become phantom rows at the
// aligned source-order position - same UX as arrays.
void alignObjectPair(DiffNode &lc, DiffNode &rc, const QString &matchKey)
{
    Q_ASSERT(lc.type == QJsonValue::Object && rc.type == QJsonValue::Object);

    const QList<int> lcPeerPath = lc.relationPath;
    const QList<int> rcPeerPath = rc.relationPath;

    QHash<QString, int> rightByKey;
    for (int i = 0; i < rc.children.size(); ++i)
        rightByKey[rc.children[i].key] = i;

    // Walk left in source order; matched pairs and left-orphan slots
    // preserve that order. Right orphans append after - their source
    // position isn't meaningful for lining up rows on the left side.
    QList<QPair<int, int>> align;
    align.reserve(lc.children.size() + rc.children.size());
    QSet<int> matchedRight;
    for (int i = 0; i < lc.children.size(); ++i)
    {
        const auto it = rightByKey.constFind(lc.children[i].key);
        if (it == rightByKey.cend())
        {
            align.append(qMakePair(i, -1));
        }
        else
        {
            align.append(qMakePair(i, it.value()));
            matchedRight.insert(it.value());
        }
    }
    for (int j = 0; j < rc.children.size(); ++j)
    {
        if (!matchedRight.contains(j))
            align.append(qMakePair(-1, j));
    }

    lc.arrayAlignment = align;
    QList<QPair<int, int>> peerAlign;
    peerAlign.reserve(align.size());
    for (const auto &p : align)
        peerAlign.append(qMakePair(p.second, p.first));
    rc.arrayAlignment = peerAlign;

    bool anyChildDiffers = false;
    for (const auto &p : align)
    {
        if (p.first == -1 && p.second != -1)
        {
            forceDiffNodeSubtreeColor(rc.children[p.second],
                                      DiffColorType::NotPresent);
            anyChildDiffers = true;
            continue;
        }
        if (p.first != -1 && p.second == -1)
        {
            forceDiffNodeSubtreeColor(lc.children[p.first],
                                      DiffColorType::NotPresent);
            anyChildDiffers = true;
            continue;
        }
        DiffNode &lch = lc.children[p.first];
        DiffNode &rch = rc.children[p.second];
        QList<int> newLc = lcPeerPath; newLc.append(p.second);
        QList<int> newRc = rcPeerPath; newRc.append(p.first);
        compareContentFirst(lch, rch, newLc, newRc, matchKey);
        if (lch.color != DiffColorType::Identical)
            anyChildDiffers = true;
    }

    lc.color = anyChildDiffers ? DiffColorType::Moderate
                               : DiffColorType::Identical;
    rc.color = lc.color;
}

// Overlay pass: after a positional walker (FullPath / PCP) has finished
// and fixColors has run, walk the left tree and, at every paired
// container (Array/Array or Object/Object), replace the walker's
// positional child pairing with LCS-style alignment via alignArrayPair
// (arrays) or alignObjectPair (objects). The subtree of a re-aligned
// container is fully rewritten (colors, relationPaths, arrayAlignment
// on both sides, phantom-worthy orphans marked NotPresent) so we don't
// descend past it. Ancestor colors are re-settled on both trees
// because a shifted pairing can demote a formerly-Moderate parent back
// to Identical (or vice versa).
bool alignPairedContainersPostPass(DiffNode &leftRoot, DiffNode &rightRoot,
                                   DiffNode &node,
                                   const QList<int> &myPath,
                                   const QString &matchKey,
                                   ProgressTracker *tracker)
{
    if (tracker && !tracker->tick())
        return false;

    const bool isContainer = (node.type == QJsonValue::Array
                              || node.type == QJsonValue::Object);
    // Root is always implicitly paired to the other root, but its
    // relationPath is empty (the correct "peer is at empty structural
    // path = the other root" value) - indistinguishable from "not
    // paired." Detect it via myPath.isEmpty() and treat it as paired.
    const bool isRoot = myPath.isEmpty();
    const bool paired = isRoot || !node.relationPath.isEmpty();
    if (isContainer && paired
            && node.arrayAlignment.isEmpty()
            && node.color != DiffColorType::NotPresent)
    {
        const QList<int> peerPath = node.relationPath;
        DiffNode *peer = navigateTo(rightRoot, peerPath);
        if (peer && peer->type == node.type
                && peer->color != DiffColorType::NotPresent)
        {
            if (node.type == QJsonValue::Array)
                alignArrayPair(node, *peer, matchKey);
            else
                alignObjectPair(node, *peer, matchKey);
            refreshAncestorColors(leftRoot,  myPath);
            refreshAncestorColors(rightRoot, peerPath);
            return true;
        }
    }

    for (int i = 0; i < node.children.size(); ++i)
    {
        QList<int> childPath = myPath;
        childPath.append(i);
        if (!alignPairedContainersPostPass(leftRoot, rightRoot, node.children[i],
                                           childPath, matchKey, tracker))
            return false;
    }
    return true;
}

// Internal compare. `tracker` may be null for the pure synchronous path.
// Returns whether the compare ran to completion (true) or was cancelled
// (false). Caller uses that to decide between finished() and cancelled().
// `arrayOverlay`: when true, after the positional walker (FullPath /
// ParentChildPair) finishes, run alignPairedContainersPostPass so
// paired Arrays AND paired Objects get LCS-style children pairing plus
// phantom rows on the opposite side.
bool compareInternal(DiffNode &left, DiffNode &right,
                     JsonDiffEngine::Mode mode,
                     ProgressTracker *tracker,
                     const QString &matchKey,
                     bool arrayOverlay)
{
    if (mode == JsonDiffEngine::Mode::FullPath)
    {
        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::BuildingPaths);
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

        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::MatchingPaths);
        if (!comparePathOneWay(leftNodes,  leftPaths,  leftIdxPaths,
                               rightNodes, rightIdx,   rightIdxPaths, tracker))
            return false;
        if (!comparePathOneWay(rightNodes, rightPaths, rightIdxPaths,
                               leftNodes,  leftIdx,    leftIdxPaths, tracker))
            return false;

        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::ComparingValues);
        if (!compareValueOneWay(leftNodes,  right, tracker)) return false;
        if (!compareValueOneWay(rightNodes, left,  tracker)) return false;

        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::ResolvingColors);
        if (!fixColors(left,  tracker)) return false;
        if (!fixColors(right, tracker)) return false;
    }
    else
    {
        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::IndexingRightTree);
        FindInRightIndex rightIndex;
        buildFindInRightIndex(right, {}, rightIndex);
        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::PairingItems);
        if (!compareModelsRecursive(left, {}, rightIndex, tracker)) return false;
        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::ResolvingColors);
        if (!fixColors(left,  tracker)) return false;
        if (!fixColors(right, tracker)) return false;
    }

    // Overlay: LCS-align each pair of paired containers. Runs AFTER
    // fixColors so it can demote ancestors that fixColors already
    // promoted to Moderate on the basis of the positional mispairing.
    if (arrayOverlay)
    {
        if (tracker) tracker->setPhase(JsonDiffEngine::Phase::AligningContainers);
        if (!alignPairedContainersPostPass(left, right, left, {},
                                           matchKey, tracker))
            return false;
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
    // Q_ENUM alone does not register the enum for queued-call argument
    // marshalling on every Qt version - CI hits "Unable to handle
    // unregistered datatype 'JsonDiffEngine::Mode'" otherwise.
    qRegisterMetaType<JsonDiffEngine::Mode>("JsonDiffEngine::Mode");
    qRegisterMetaType<JsonDiffEngine::Phase>("JsonDiffEngine::Phase");
}

JsonDiffEngine::~JsonDiffEngine() = default;

void JsonDiffEngine::compare(DiffNode &left, DiffNode &right, Mode mode,
                             const QString &matchKey, bool arrayOverlay)
{
    // Synchronous path: no tracker, no progress, no cancel.
    compareInternal(left, right, mode, nullptr, matchKey, arrayOverlay);
}

// -----------------------------------------------------------------------
// Phase A - targeted edit-and-recompare
// -----------------------------------------------------------------------

namespace
{

// Navigate from `root` along `path` (sequence of child indices).
// Returns nullptr if any step goes out of bounds. Path may be empty,
// in which case the root itself is returned.
DiffNode *navigateTo(DiffNode &root, const QList<int> &path)
{
    DiffNode *cur = &root;
    for (int step : path)
    {
        if (step < 0 || step >= cur->children.size())
            return nullptr;
        cur = &cur->children[step];
    }
    return cur;
}

// True if a single matched pair (one node per side, already known to
// reference each other) is "the same" from the algorithm's point of
// view - mirrors the value/childcount checks compareValueOneWay and
// findInRight use, so a recomputed color matches what a full compare
// would produce for the same pair.
bool pairLooksIdentical(const DiffNode &L, const DiffNode &R)
{
    if (L.type != R.type)
        return false;
    if (L.type == QJsonValue::Object || L.type == QJsonValue::Array)
        return L.children.size() == R.children.size();
    if (L.type == QJsonValue::Null)
        return L.key == R.key;  // matches findInRight's Null special case
    return L.value == R.value;  // scalars (Bool/Double/String)
}

// After a leaf changed color, walk from leafPath upward refreshing
// each ancestor's Moderate/Identical state. The rule mirrors the
// original fixColors() but adds *demotion*: if all of an ancestor's
// descendants are now Identical/None/NotPresent, the ancestor can go
// back to Identical from Moderate. Huge and NotPresent ancestors are
// not touched (they were set by something other than aggregation).
void refreshAncestorColors(DiffNode &root, const QList<int> &leafPath)
{
    // Collect ancestor pointers from root down to (but not including)
    // the leaf itself. ancestors[0] is the tree root, ancestors[N-1]
    // is the leaf's direct parent.
    QList<DiffNode*> ancestors;
    DiffNode *cur = &root;
    ancestors.append(cur);
    for (int i = 0; i + 1 < leafPath.size(); ++i)
    {
        if (leafPath[i] < 0 || leafPath[i] >= cur->children.size())
            return;
        cur = &cur->children[leafPath[i]];
        ancestors.append(cur);
    }

    // Walk from deepest ancestor up - each level's decision depends on
    // the level below having already settled.
    for (int i = ancestors.size() - 1; i >= 0; --i)
    {
        DiffNode *node = ancestors[i];
        if (node->color == DiffColorType::Huge
                || node->color == DiffColorType::NotPresent)
            continue;

        bool hasDiffDescendant = false;
        for (const DiffNode &child : node->children)
        {
            if (child.color != DiffColorType::Identical
                    && child.color != DiffColorType::None
                    && child.color != DiffColorType::NotPresent)
            {
                hasDiffDescendant = true;
                break;
            }
        }
        node->color = hasDiffDescendant
            ? DiffColorType::Moderate
            : DiffColorType::Identical;
    }
}

} // anonymous namespace

qint64 JsonDiffEngine::weightOf(const DiffNode &node)
{
    return weightOfString(canonicalValueString(node));
}

QList<QPair<int, int>>
JsonDiffEngine::alignByWeightWithKey(const QList<DiffNode> &left,
                                     const QList<DiffNode> &right,
                                     const QString &matchKey)
{
    // Accept a comma-separated list of candidate field names. First
    // match on any of them wins. This lets one user-typed key cover
    // arrays that use different id-field names at different levels
    // (id / eventId / marketId / selectionId).
    QStringList keys;
    for (const QString &k : matchKey.split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        const QString t = k.trimmed();
        if (!t.isEmpty()) keys.append(t);
    }
    // Two-bucket identity. Items that resolve one of the requested
    // keys use that field's weight directly. Items that don't (Object
    // with no key found, or non-Object entries) fall back to the
    // full-node weight XOR a fixed mask - same-content items in that
    // bucket still pair with each other, but they can't cross-pair
    // with items that DID resolve a key (which would be the "stupid
    // key-weight vs full-node-weight comparison" the user objected
    // to).
    static constexpr qint64 NO_KEY_MASK = 0x5555555555555555LL;
    auto identityOf = [&](const DiffNode &n) -> qint64 {
        if (!keys.isEmpty() && n.type == QJsonValue::Object)
        {
            for (const QString &key : keys)
                for (const auto &c : n.children)
                    if (c.key == key)
                        return c.weight;
            return n.weight ^ NO_KEY_MASK;
        }
        return n.weight;
    };
    QList<qint64> lw, rw;
    lw.reserve(left.size());
    rw.reserve(right.size());
    for (const auto &n : left)  lw.append(identityOf(n));
    for (const auto &n : right) rw.append(identityOf(n));
    return alignByWeight(lw, rw);
}

QList<QPair<int, int>>
JsonDiffEngine::alignByWeight(const QList<qint64> &left,
                              const QList<qint64> &right)
{
    // Unordered bipartite matching by weight. Group items on each side
    // by weight, then pair k-th occurrence with k-th occurrence. Items
    // that have a matching weight on the other side always pair -
    // position (order) is ignored. Items with no weight peer become
    // orphans. Used so "same JSON, arrays sorted differently" pairs
    // 100% of items instead of collapsing under LCS's order rule.
    //
    // Output layout:
    //   1. matched pairs sorted by leftIdx (both != -1)
    //   2. left orphans (leftIdx, -1) in leftIdx order
    //   3. right orphans (-1, rightIdx) in rightIdx order
    //
    // Consumers of arrayAlignment must NOT interpret the sequence as a
    // positional merge - real matches live at their original leftIdx,
    // and phantoms are appended at the end of the model.
    QHash<qint64, QList<int>> leftByWeight, rightByWeight;
    for (int i = 0; i < left.size(); ++i)  leftByWeight[left[i]].append(i);
    for (int j = 0; j < right.size(); ++j) rightByWeight[right[j]].append(j);

    QList<QPair<int, int>> matched;
    QSet<int> leftMatched, rightMatched;
    for (auto it = leftByWeight.cbegin(); it != leftByWeight.cend(); ++it)
    {
        const auto rit = rightByWeight.constFind(it.key());
        if (rit == rightByWeight.cend()) continue;
        const auto &lp = it.value();
        const auto &rp = rit.value();
        const int n = std::min(lp.size(), rp.size());
        for (int k = 0; k < n; ++k)
        {
            matched.append(qMakePair(lp[k], rp[k]));
            leftMatched.insert(lp[k]);
            rightMatched.insert(rp[k]);
        }
    }
    std::sort(matched.begin(), matched.end());

    QList<QPair<int, int>> out;
    out.reserve(left.size() + right.size());
    out.append(matched);
    for (int i = 0; i < left.size(); ++i)
        if (!leftMatched.contains(i))
            out.append(qMakePair(i, -1));
    for (int j = 0; j < right.size(); ++j)
        if (!rightMatched.contains(j))
            out.append(qMakePair(-1, j));
    return out;
}

QJsonValue JsonDiffEngine::toJsonValue(const DiffNode &node)
{
    switch (node.type)
    {
        case QJsonValue::Object:
        {
            QJsonObject obj;
            for (const DiffNode &c : node.children)
                obj.insert(c.key, toJsonValue(c));
            return obj;
        }
        case QJsonValue::Array:
        {
            QJsonArray arr;
            for (const DiffNode &c : node.children)
                arr.append(toJsonValue(c));
            return arr;
        }
        case QJsonValue::String:
            return QJsonValue(node.value);
        case QJsonValue::Double:
            return QJsonValue(node.value.toDouble());
        case QJsonValue::Bool:
        {
            // Bool snapshots are stringified by QJsonModel as "true" /
            // "false". Anything else means the snapshot is malformed -
            // warn so we don't silently coerce garbage to false.
            const bool isTrue  = node.value.compare(QStringLiteral("true"),
                                                    Qt::CaseInsensitive) == 0;
            const bool isFalse = node.value.compare(QStringLiteral("false"),
                                                    Qt::CaseInsensitive) == 0;
            if (!isTrue && !isFalse)
                qWarning() << "JsonDiffEngine::toJsonValue: Bool node has "
                              "non-canonical value" << node.value
                           << "- treating as false";
            return QJsonValue(isTrue);
        }
        case QJsonValue::Null:
            return QJsonValue(QJsonValue::Null);
        case QJsonValue::Undefined:
        default:
            // Undefined should never reach here from a well-formed
            // snapshot; warn rather than silently substituting Null.
            qWarning() << "JsonDiffEngine::toJsonValue: unexpected type"
                       << int(node.type) << "- writing Null";
            return QJsonValue(QJsonValue::Null);
    }
}

void JsonDiffEngine::copyPeer(DiffNode &target, const DiffNode &source)
{
    // Preserve target.key - it identifies position in target's parent.
    // color/relationPath are deliberately untouched; recomparePair()
    // is the next step the caller will run and it will set them.
    target.type     = source.type;
    target.value    = source.value;
    target.children = source.children;
}

void JsonDiffEngine::recomparePair(DiffNode &leftRoot,  const QList<int> &leftPath,
                                   DiffNode &rightRoot, const QList<int> &rightPath,
                                   Mode mode)
{
    Q_UNUSED(mode);  // reserved for mode-specific edge cases (Phase A: not needed)

    DiffNode *L = navigateTo(leftRoot,  leftPath);
    DiffNode *R = navigateTo(rightRoot, rightPath);
    if (!L || !R)
        return;

    const bool identical = pairLooksIdentical(*L, *R);
    L->color = identical ? DiffColorType::Identical : DiffColorType::Huge;
    R->color = identical ? DiffColorType::Identical : DiffColorType::Huge;

    refreshAncestorColors(leftRoot,  leftPath);
    refreshAncestorColors(rightRoot, rightPath);
}

// -----------------------------------------------------------------------
// Phase B - append / remove for NotPresent rows
// -----------------------------------------------------------------------

namespace
{

// Walk two same-shape subtrees in parallel, marking each pair Identical
// and mirroring relationPath. Used right after insertPeer's deep-copy
// so apply() can light up idxRelation across the whole inserted subtree
// - not just its root.
void linkSubtreesRecursively(DiffNode &a, const QList<int> &aPath,
                             DiffNode &b, const QList<int> &bPath)
{
    a.color = DiffColorType::Identical;
    b.color = DiffColorType::Identical;
    a.relationPath = bPath;
    b.relationPath = aPath;
    // copy preserves shape - children sizes match. qMin guards in case
    // the caller hands us trees that drifted apart somehow.
    const int n = qMin(a.children.size(), b.children.size());
    for (int i = 0; i < n; ++i)
    {
        QList<int> aChild = aPath; aChild.append(i);
        QList<int> bChild = bPath; bChild.append(i);
        linkSubtreesRecursively(a.children[i], aChild, b.children[i], bChild);
    }
}

// After srcRoot.children[parentPath][idx] was removed, fix every node
// in otherRoot whose relationPath either:
//   - referenced the removed subtree (== parentPath + idx + suffix) →
//     orphan it: clear relationPath, set color to NotPresent.
//   - referenced a *later* sibling (parentPath + k + suffix where
//     k > idx) → decrement k by 1 to account for the shift-left.
void fixupRelationPathsAfterRemoval(DiffNode &otherRoot,
                                    const QList<int> &removedParentPath,
                                    int removedIdx,
                                    QList<QList<int>> &orphanPaths)
{
    // Track the current path through otherRoot so we can record each
    // orphan's actual position. removePeer needs those paths to walk
    // the right ancestor chains on the other side - using the src-side
    // parentPath was only accurate when the trees lined up index-for-
    // index, which Phase-B's paired compares can't assume.
    QList<int> current;
    std::function<void(DiffNode&)> visit = [&](DiffNode &n)
    {
        if (!n.relationPath.isEmpty()
                && n.relationPath.size() > removedParentPath.size()
                && std::equal(removedParentPath.begin(),
                              removedParentPath.end(),
                              n.relationPath.begin()))
        {
            int &k = n.relationPath[removedParentPath.size()];
            if (k == removedIdx)
            {
                n.relationPath.clear();
                n.color = DiffColorType::NotPresent;
                orphanPaths.append(current);
            }
            else if (k > removedIdx)
            {
                k = k - 1;
            }
        }
        for (int i = 0; i < n.children.size(); ++i)
        {
            current.append(i);
            visit(n.children[i]);
            current.removeLast();
        }
    };
    visit(otherRoot);
}

} // anonymous namespace

bool JsonDiffEngine::resolveDstParentPath(const DiffNode &srcRoot,
                                          const QList<int> &srcPath,
                                          QList<int> &dstParentPath)
{
    if (srcPath.isEmpty())
        return false;                       // can't push root
    if (srcPath.size() == 1)
    {
        // src parent IS the source root → dst parent is the other root.
        dstParentPath.clear();
        return true;
    }
    // Walk to source parent.
    const DiffNode *parent = &srcRoot;
    for (int i = 0; i + 1 < srcPath.size(); ++i)
    {
        const int step = srcPath[i];
        if (step < 0 || step >= parent->children.size())
            return false;
        parent = &parent->children[step];
    }
    if (parent->relationPath.isEmpty())
        return false;                       // parent has no peer
    dstParentPath = parent->relationPath;
    return true;
}

QList<int> JsonDiffEngine::insertPeer(DiffNode &srcRoot,
                                      const QList<int> &srcPath,
                                      DiffNode &dstRoot,
                                      const QList<int> &dstParentPath,
                                      Mode mode)
{
    Q_UNUSED(mode);

    DiffNode *src = navigateTo(srcRoot, srcPath);
    DiffNode *dstParent = navigateTo(dstRoot, dstParentPath);
    if (!src || !dstParent)
        return {};
    if (dstParent->type != QJsonValue::Object
            && dstParent->type != QJsonValue::Array)
        return {};

    // Deep-copy the source subtree, then patch the key for the
    // destination context (arrays index by position, objects keep the
    // source's key).
    DiffNode copy = *src;
    if (dstParent->type == QJsonValue::Array)
        copy.key = QString::number(dstParent->children.size());

    dstParent->children.append(std::move(copy));

    QList<int> dstPath = dstParentPath;
    dstPath.append(dstParent->children.size() - 1);

    // Cross-link the whole inserted subtree to its source counterpart
    // so apply() sets idxRelation everywhere, not just at the root.
    linkSubtreesRecursively(*src, srcPath,
                            dstParent->children.last(), dstPath);

    // Refresh both sides' ancestor colors - src's chain may now be
    // back to Identical (its row went from NotPresent to Identical),
    // dst's chain may have lost its last diff descendant.
    refreshAncestorColors(srcRoot, srcPath);
    refreshAncestorColors(dstRoot, dstPath);
    return dstPath;
}

bool JsonDiffEngine::resnapshotSubtree(DiffNode &myRoot,
                                       const QList<int> &myPath,
                                       QJsonModel *myModel,
                                       DiffNode &peerRoot)
{
    if (myPath.isEmpty() || !myModel) return false;
    DiffNode *me = navigateTo(myRoot, myPath);
    if (!me) return false;

    // Navigate the model to the same path.
    QModelIndex idx;
    for (int step : myPath)
        idx = myModel->index(step, 0, idx);
    if (!idx.isValid()) return false;

    // Wipe and re-walk children from the model. snapshotChildren is
    // the same helper snapshot() uses, so the rebuilt subtree mirrors
    // what a fresh Compare would see.
    me->children.clear();
    snapshotChildren(myModel, idx, *me);

    // New children have no peers on the other side - mark the whole
    // re-snapshotted subtree NotPresent so apply() paints them as
    // "missing on the other side" until the user re-runs Compare.
    std::function<void(DiffNode&)> markAllNotPresent = [&](DiffNode &n)
    {
        n.color = DiffColorType::NotPresent;
        n.relationPath.clear();
        for (DiffNode &c : n.children) markAllNotPresent(c);
    };
    for (DiffNode &c : me->children) markAllNotPresent(c);

    // Orphan any peer pointer that referenced something INSIDE the
    // old subtree. The peer of `me` itself (relationPath == myPath)
    // is left intact - recomparePair will re-evaluate that pair's
    // color after the call returns.
    std::function<void(DiffNode&)> orphanDescendantPeers = [&](DiffNode &n)
    {
        if (!n.relationPath.isEmpty()
                && n.relationPath.size() > myPath.size()
                && std::equal(myPath.begin(), myPath.end(),
                              n.relationPath.begin()))
        {
            n.relationPath.clear();
            n.color = DiffColorType::NotPresent;
        }
        for (DiffNode &c : n.children) orphanDescendantPeers(c);
    };
    orphanDescendantPeers(peerRoot);
    return true;
}

bool JsonDiffEngine::detachPair(DiffNode &myRoot, const QList<int> &myPath,
                                DiffNode &peerRoot, Mode mode)
{
    Q_UNUSED(mode);
    if (myPath.isEmpty()) return false;
    DiffNode *me = navigateTo(myRoot, myPath);
    if (!me) return false;

    const QList<int> peerPath = me->relationPath;
    me->color = DiffColorType::NotPresent;
    me->relationPath.clear();

    if (!peerPath.isEmpty())
    {
        DiffNode *peer = navigateTo(peerRoot, peerPath);
        if (peer)
        {
            peer->color = DiffColorType::NotPresent;
            peer->relationPath.clear();
            refreshAncestorColors(peerRoot, peerPath);
        }
    }
    refreshAncestorColors(myRoot, myPath);
    return true;
}

bool JsonDiffEngine::removePeer(DiffNode &srcRoot, const QList<int> &srcPath,
                                DiffNode &otherRoot, Mode mode)
{
    Q_UNUSED(mode);

    if (srcPath.isEmpty())
        return false;                       // can't remove the root

    // Walk to the source parent and identify the removed index.
    DiffNode *parent = &srcRoot;
    for (int i = 0; i + 1 < srcPath.size(); ++i)
    {
        const int step = srcPath[i];
        if (step < 0 || step >= parent->children.size())
            return false;
        parent = &parent->children[step];
    }
    const int idx = srcPath.last();
    if (idx < 0 || idx >= parent->children.size())
        return false;

    QList<int> parentPath = srcPath;
    parentPath.removeLast();

    // Remove the child. We don't track per-node peer cleanup here -
    // fixupRelationPathsAfterRemoval does that across the whole other
    // tree in one walk.
    parent->children.removeAt(idx);

    // Fix peer pointers on the other side: anything pointing into the
    // removed subtree gets orphaned to NotPresent (and its actual path
    // recorded for ancestor refresh below); anything pointing at a
    // later sibling slides down by one.
    QList<QList<int>> orphanPaths;
    fixupRelationPathsAfterRemoval(otherRoot, parentPath, idx, orphanPaths);

    // For arrays, the survivors past `idx` keep their old stringified-
    // index keys ("2","3"…) even though they now sit at positions
    // (1, 2, …). QJsonModel::removeRowAt fixes this on the model; the
    // snapshot must mirror so a subsequent compare uses the same path
    // strings on both sides (the engine's FullPath matcher includes
    // keys in the path).
    if (parent->type == QJsonValue::Array)
    {
        for (int i = idx; i < parent->children.size(); ++i)
            parent->children[i].key = QString::number(i);
    }

    // Re-apply the original compareValue rule: paired Object/Array
    // containers with mismatched child counts BOTH go Huge (matches
    // main-branch semantics on the next Compare). This is the
    // edit-time replay of that rule - without it, an Object that
    // just lost a child while its peer kept everything looked
    // Identical or Moderate, which the user reads as "no diff" even
    // though one object is structurally a subset of the other.
    DiffNode *srcParent = navigateTo(srcRoot, parentPath);
    DiffNode *dstParent = nullptr;
    QList<int> dstParentPath;
    if (srcParent && !srcParent->relationPath.isEmpty())
    {
        dstParentPath = srcParent->relationPath;
        dstParent     = navigateTo(otherRoot, dstParentPath);
    }
    const bool isContainerPair = srcParent && dstParent
            && (srcParent->type == QJsonValue::Object
                    || srcParent->type == QJsonValue::Array);
    const bool sizesDiffer = isContainerPair
            && srcParent->children.size() != dstParent->children.size();

    if (sizesDiffer)
    {
        srcParent->color = DiffColorType::Huge;
        dstParent->color = DiffColorType::Huge;
    }

    // Refresh ancestors on both sides. The src side walks from
    // srcPath; the other side from each orphan's actual otherRoot
    // path (the pre-fix "approximate" walk used src-side indices,
    // which landed on an unrelated subtree under ParentChildPair).
    refreshAncestorColors(srcRoot, srcPath);
    if (sizesDiffer)
    {
        // dstParent just changed to Huge - refresh its ancestors
        // explicitly so they promote to Moderate. refreshAncestorColors
        // skips Huge nodes themselves, so passing dstParentPath as the
        // leafPath visits dstParent's ancestors without touching it.
        refreshAncestorColors(otherRoot, dstParentPath);
    }
    for (const QList<int> &p : orphanPaths)
        refreshAncestorColors(otherRoot, p);
    return true;
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
    // Cancel state is NOT reset here - caller (orchestrator on the main
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
    // approximate - actual costs vary, especially ParentChild where
    // findInRight is unbounded - but they make the dialog move at all.
    // Overlay adds one extra walk of the left tree at the end.
    const int leftSize  = treeSize(*left);
    const int rightSize = treeSize(*right);
    int total = (mode == Mode::FullPath)
        ? 4 * (leftSize + rightSize)
        : (2 * leftSize + rightSize);
    if (mArrayOverlay)
        total += leftSize;

    ProgressTracker tracker(
        total,
        &mCancelRequested,
        [this](int done, int t) { emit progressed(done, t); },
        [this](JsonDiffEngine::Phase phase) { emit phaseChanged(phase); }
    );

    // Compute in place on the heap-allocated DiffNodes; no tree copies.
    const bool ranToCompletion = compareInternal(*left, *right, mode, &tracker,
                                                 mMatchKey, mArrayOverlay);

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
