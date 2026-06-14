/* Author: Yuriy Kuzin
 * Description: implementation of JsonDiffEngine.
 *   Ports the original QJsonDiff comparison algorithm onto DiffNode
 *   trees. Behaviour is preserved exactly — including a couple of
 *   harmless-by-accident expressions in the original — so the existing
 *   widget tests stay green through this refactor.
 */
#include "jsondiffengine.h"

#include "qjsonmodel.h"
#include "qjsonitem.h"

#include <QModelIndex>

namespace
{

// -----------------------------------------------------------------------
// Snapshot builder helpers
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
//  - paths:   the same path-string format the old QJsonModel::jsonPath()
//             produced ("root(Object)->k(Double)"), so contains() lookups
//             match the original
//  - nodes:   parallel list of DiffNode pointers
//  - idxPaths: parallel list of QList<int> structural paths (used to
//             cross-link via relationPath in the result)
void buildPathList(DiffNode &node,
                   const QString &parentPath,
                   const QList<int> &parentIdxPath,
                   QStringList &paths,
                   QList<DiffNode*> &nodes,
                   QList<QList<int>> &idxPaths)
{
    for (int i = 0; i < node.children.size(); ++i)
    {
        DiffNode &child = node.children[i];
        QString childPath = parentPath + "->" + child.key + "(" + typeName(child.type) + ")";
        QList<int> childIdxPath = parentIdxPath;
        childIdxPath.append(i);

        paths.append(childPath);
        nodes.append(&child);
        idxPaths.append(childIdxPath);

        buildPathList(child, childPath, childIdxPath, paths, nodes, idxPaths);
    }
}

// -----------------------------------------------------------------------
// FullPath mode
// -----------------------------------------------------------------------

void comparePathOneWay(QList<DiffNode*> &myNodes,
                       const QStringList &myPaths,
                       const QList<QList<int>> &myIdxPaths,
                       const QList<DiffNode*> &otherNodes,
                       const QStringList &otherPaths,
                       const QList<QList<int>> &otherIdxPaths)
{
    for (int i = 0; i < myPaths.size(); ++i)
    {
        DiffNode *node = myNodes[i];
        if (node->color != DiffColorType::Identical)
        {
            int j = otherPaths.indexOf(myPaths[i]);
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
}

void compareValueOneWay(QList<DiffNode*> &myNodes,
                        DiffNode &otherRoot)
{
    for (int i = 0; i < myNodes.size(); ++i)
    {
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
}

// -----------------------------------------------------------------------
// ParentChild mode
// -----------------------------------------------------------------------

// Search the entire right tree for the first node that matches itemLeft's
// (parent.key, key, type) — same semantics as the original
// findIndexInModel.
//
// `rightParent` is the parent of `right` (needed because the original
// algorithm reads itemLeft->parent()->key() and item->parent()->key()).
// `leftPath` is the structural path of itemLeft, recorded as the right
// peer's relationPath when a match happens (and vice versa).
int findInRight(DiffNode &itemLeft,
                const QList<int> &leftPath,
                const QString &leftParentKey,
                DiffNode &right,
                DiffNode *rightParent,
                const QList<int> &rightPath)
{
    Q_UNUSED(rightParent);
    Q_UNUSED(rightPath);

    for (int i = 0; i < right.children.size(); ++i)
    {
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
                            candidate, &right, candidatePath);
        if (r == 0) return 0;
    }
    return -1;
}

void compareModelsRecursive(DiffNode &left,
                            const QList<int> &leftPath,
                            DiffNode &rightRoot)
{
    for (int i = 0; i < left.children.size(); ++i)
    {
        DiffNode &child = left.children[i];
        QList<int> childPath = leftPath;
        childPath.append(i);

        if (child.color == DiffColorType::None)
        {
            findInRight(child, childPath, left.key, rightRoot, nullptr, {});
        }
        compareModelsRecursive(child, childPath, rightRoot);
    }
}

// -----------------------------------------------------------------------
// Common: post-pass that promotes parents of diffs to Moderate and
// converts untouched None items to NotPresent. Same as the original
// fixColors().
// -----------------------------------------------------------------------

void fixColors(DiffNode &node)
{
    for (DiffNode &child : node.children)
    {
        fixColors(child);

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

void JsonDiffEngine::compare(DiffNode &left, DiffNode &right, Mode mode)
{
    if (mode == Mode::FullPath)
    {
        QStringList                leftPaths;
        QList<DiffNode*>           leftNodes;
        QList<QList<int>>          leftIdxPaths;
        const QString prefix = "root(" + typeName(left.type) + ")";
        buildPathList(left, prefix, {}, leftPaths, leftNodes, leftIdxPaths);

        QStringList                rightPaths;
        QList<DiffNode*>           rightNodes;
        QList<QList<int>>          rightIdxPaths;
        const QString rprefix = "root(" + typeName(right.type) + ")";
        buildPathList(right, rprefix, {}, rightPaths, rightNodes, rightIdxPaths);

        comparePathOneWay(leftNodes,  leftPaths,  leftIdxPaths,
                          rightNodes, rightPaths, rightIdxPaths);
        comparePathOneWay(rightNodes, rightPaths, rightIdxPaths,
                          leftNodes,  leftPaths,  leftIdxPaths);
        compareValueOneWay(leftNodes,  right);
        compareValueOneWay(rightNodes, left);
    }
    else
    {
        compareModelsRecursive(left, {}, right);
    }

    fixColors(left);
    fixColors(right);
}
