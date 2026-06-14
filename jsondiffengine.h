/* Author: Yuriy Kuzin
 * Description: pure compare logic for two JSON trees.
 *   No QWidget / QAbstractItemModel deps — only QtCore.
 *   Phase 1: runs on the calling thread, synchronously.
 *   Phase 2: will be moved onto a QThread and made cancellable +
 *            progress-reporting (kept Q_OBJECT-able in mind here).
 */
#ifndef JSONDIFFENGINE_H
#define JSONDIFFENGINE_H

#include <QString>
#include <QList>
#include <QJsonValue>

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

class JsonDiffEngine
{
public:
    enum class Mode
    {
        FullPath,           // default, fast: match by absolute key+type path
        ParentChildPair     // slow: first parent.key+key+type match wins
    };

    // Build a snapshot of a model. Main thread only.
    static DiffNode snapshot(QJsonModel *model);

    // Run the compare. Phase 1: synchronous, fills color + relationPath
    // on both trees in place. Behaviour is preserved exactly from the
    // original QJsonDiff implementation so the widget tests stay green.
    static void compare(DiffNode &left, DiffNode &right, Mode mode);

    // Push the snapshot's color + relationPath state back onto the
    // model's tree items (colorType, idxRelation). otherModel is needed
    // to turn relationPath into a QModelIndex on the opposite side.
    // Main thread only. Emits layoutChanged() so views repaint.
    static void apply(const DiffNode &snap, QJsonModel *model, QJsonModel *otherModel);
};

#endif // JSONDIFFENGINE_H
