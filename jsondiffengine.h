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

    explicit JsonDiffEngine(QObject *parent = nullptr);
    ~JsonDiffEngine() override;

    // --- Main-thread static helpers (read/write the model) -----------

    // Build a snapshot of a model. Main thread only.
    static DiffNode snapshot(QJsonModel *model);

    // Push snapshot.color + snapshot.relationPath back onto the model's
    // QJsonTreeItems. Main thread only. Emits layoutChanged() so views
    // repaint.
    static void apply(const DiffNode &snap, QJsonModel *model, QJsonModel *otherModel);

    // --- Synchronous compare (pure compute) --------------------------

    // Fills color + relationPath in place. Used by tests, used as the
    // body of compareAsync(). No thread or signal interaction.
    static void compare(DiffNode &left, DiffNode &right, Mode mode);

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
    void finished(QSharedPointer<DiffNode> left, QSharedPointer<DiffNode> right);
    void cancelled();

private:
    std::atomic<bool> mCancelRequested{false};
};

#endif // JSONDIFFENGINE_H
