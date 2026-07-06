/* Author: Yuriy Kuzin
 */
#ifndef QJSONDIFF_H
#define QJSONDIFF_H

#include <QObject>
#include <QWidget>
#include <QHBoxLayout>
#include <QMargins>
#include <QPushButton>
#include <QModelIndex>
#include <QScrollBar>
#include <QSpacerItem>
#include <QShortcut>
#include <QElapsedTimer>
#include <QPointer>

#include "qjsoncontainer.h"
#include"qjsonitem.h"
#include"qjsonmodel.h"
#include "jsondiffengine.h"

class QPaintEvent;
class QThread;
class QDialog;
class QProgressBar;
class QLabel;
class QPushButton;
class QAction;
class QLineEdit;
class QComboBox;

class QJsonDiff : public QWidget
{
    Q_OBJECT

public:
    explicit QJsonDiff(QWidget *parent = nullptr);
    ~QJsonDiff() override;
    QJsonContainer *left_cont;
    QJsonContainer *right_cont;
    void loadJsonLeft(QJsonDocument data);
    void loadJsonLeft(const QString& data);
    void loadJsonRight(QJsonDocument data);
    void loadJsonRight(const QString &data);
    QTreeView* getLeftTreeView();
    QTreeView* getRightTreeView();
    QJsonModel* getLeftJsonModel();
    QJsonModel* getRightJsonModel();
    QGridLayout *qjsoncontainer_layout;
    QGroupBox *container_left_groupbox;
    QGroupBox *container_right_groupbox;
    QGroupBox * common_groupbox;
    QVBoxLayout *common_layout;
    QGroupBox *button_groupbox;
    QGridLayout *button_layout;
    QGroupBox *compare_groupbox;
    QGridLayout *compare_layout;
    QPushButton *compare_pushbutton;
    QShortcut *compare_shortcut;
    void expandIt();
    QCheckBox *syncScroll_checkbox;
    // Compare controls:
    // modeCombo: Full Path / Parent+Child. Drives which positional
    //   walker the engine runs.
    // arrayOverlay_checkbox: "Smart Array" - layer LCS-style children
    //   pairing on top of the positional mode, for both arrays and
    //   objects, with phantom rows on the opposite side.
    // matchKey_lineEdit: optional field name(s) that the array LCS
    //   prefers when matching arrays of objects (weight fallback for
    //   items without the field).
    QComboBox *modeCombo               = nullptr;
    QCheckBox *arrayOverlay_checkbox   = nullptr;
    QLineEdit *matchKey_lineEdit       = nullptr;
    QSpacerItem *checkboxSpacer;

    int prevLeftScroll;
    int prevRightScroll;
    // Synchronous compare on the calling thread. Used by integrators
    // and tests. The Compare button uses the threaded path instead
    // (see on_compare_pushbutton_clicked).
    void startComparison();
    void setBrowseVisible(bool state);
    void showJsonButtonPosition();

    // Enable inline diff editing: tree-views become editable and the
    // overlay "push selected → peer" buttons become available on rows
    // whose colorType is Huge and whose idxRelation is valid. Off by
    // default (preserves the read-only contract for integrators).
    void setDiffEditable(bool editable);
    bool isDiffEditable() const;

signals:
    void sLoadRightJsonFile(const QString &target);
    void sLoadLeftJsonFile(const QString &target);
    void sRightJsonFileLoaded(const QString &path);
    void sLeftJsonFileLoaded(const QString &path);

public slots:
    // Push the value at the currently-selected row on one side INTO
    // the matching row on the other side. For Huge pairs (Phase A)
    // overwrites the peer in place. For NotPresent rows (Phase B) the
    // peer is created on the other side by appending into the matched
    // parent. Public so integrators can bind them to keyboard
    // shortcuts; the toolbar arrow buttons trigger these internally.
    // Returns true on success, false when nothing was done (pre-Compare,
    // no selection, root, chained-missing peer). Queued-connection
    // callers can ignore the return; direct callers can use it to know
    // whether to update their own UI.
    bool pushLeftSelectionToRight();
    bool pushRightSelectionToLeft();

    // Phase B: remove the currently-selected row on the corresponding
    // side. Top-level (root) deletion is rejected. If the row had a
    // peer on the other side, that peer becomes NotPresent. Returns
    // true on success, false on rejected/no-op.
    bool deleteLeftSelection();
    bool deleteRightSelection();

    void on_compare_pushbutton_clicked();
    void on_lefttreeview_clicked(const QModelIndex&);
    void on_righttreeview_clicked(const QModelIndex&);
    void on_lefttreeview_scroll_valuechanged(int);
    void on_righttreeview_scroll_valuechanged(int);
    void reinitRightModel();
    void reinitLeftModel();
    void loadRightJsonFile(const QString &target);
    void loadLeftJsonFile(const QString &target);
    void rightJsonFileLoaded(const QString &path);
    void leftJsonFileLoaded(const QString &path);

private slots:
    void onCompareFinished(QSharedPointer<DiffNode> left, QSharedPointer<DiffNode> right);
    void onCompareCancelled();
    void onCompareProgressed(int done, int total);
    // Runs the main-thread pre-work (reInitModel + snapshot) and posts
    // compareAsync to the worker. Split out of on_compare_pushbutton_clicked
    // so it can be QTimer::singleShot(0)'d - that way the just-shown
    // progress dialog gets an event-loop turn to actually paint before
    // we start blocking the main thread with the pre-work.
    void runComparePreworkAndDispatch();

    // Refresh push + delete button visibility when the selection changes.
    void updateLeftPushAction();
    void updateRightPushAction();
    void updateLeftDeleteAction();
    void updateRightDeleteAction();

    // Inline-edit feedback: when user edits a cell on either side,
    // sync the snapshot and recompare the affected pair so the color
    // updates without requiring a full re-compare.
    void onLeftModelDataChanged(const QModelIndex &topLeft,
                                const QModelIndex &bottomRight,
                                const QList<int> &roles);
    void onRightModelDataChanged(const QModelIndex &topLeft,
                                 const QModelIndex &bottomRight,
                                 const QList<int> &roles);

    // Structural-insert feedback for item DnD (and any other
    // appendChildFromJson / insertChildFromJson caller while the diff
    // is live). New rows are added to the snapshot as NotPresent
    // (gray) - they have no peer on the other side. If the drop
    // parent itself was paired, its color is refreshed since its
    // child count diverged.
    void onLeftRowsInserted(const QModelIndex &parent, int first, int last);
    void onRightRowsInserted(const QModelIndex &parent, int first, int last);

    // Fires from inside removeRowAt before the items are destroyed.
    // Handles any delete path (context-menu, toolbar arrow, ...).
    void onLeftRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onRightRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onLeftRowsRemoved(const QModelIndex &parent, int first, int last);
    void onRightRowsRemoved(const QModelIndex &parent, int first, int last);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    // Worker thread + engine used by the Compare button's async path.
    // startComparison() (the public API) stays synchronous and does
    // not touch these.
    JsonDiffEngine  *mEngine = nullptr;
    QThread         *mWorkerThread = nullptr;
    // Custom modal progress dialog - a QDialog with a QProgressBar +
    // label + Cancel button, no QProgressDialog. QProgressDialog's
    // internal state machine (shown_once flag, forceTimer interaction,
    // setValue's event-loop pump, autoClose triggers) made it flaky
    // across the reset → show → progress path we need each Compare.
    // QPointer auto-nulls if the dialog is destroyed while the queued
    // finished() drains inside a repaint pump.
    QPointer<QDialog>       mProgressDialog;
    QProgressBar           *mProgressBar   = nullptr;
    QLabel                 *mProgressLabel = nullptr;
    QPushButton            *mProgressCancel = nullptr;
    bool             mCompareCancelledByUser = false;
    // Re-entrancy guard against rapid re-click on the Compare button (or
    // the ALT+C shortcut) while a previous compare is still in flight.
    // Without it, QProgressDialog::setValue() pumps the event loop on a
    // modal dialog before show() runs - that pump can deliver a queued
    // second click, re-enter on_compare_pushbutton_clicked, queue a
    // second compareAsync, and leave the dialog in a state where finished
    // arrives before the second show() and the dialog hangs visible.
    bool             mComparing = false;
    QElapsedTimer    mCompareElapsed;             // Phase 3 polish
    QString          mCurrentPhase;               // last phaseChanged emission

    void setupCompareWorker();
    void teardownCompareWorker();
    // Builds the progress-dialog label from mCurrentPhase + the
    // elapsed time on mCompareElapsed. "Comparing values… (12s)" etc.
    QString formatProgressLabel() const;
    // Common back-half for both dataChanged slots: walk to the snapshot
    // node at the changed model index, mirror key/type/value, then run
    // the appropriate engine helper (recomparePair for content drift,
    // detachPair for a key rename) and re-apply to both models.
    void recomputeAfterUserEdit(const QModelIndex &idx, bool onLeft);

    // Back-half for both rowsInserted slots: build a fresh DiffNode
    // for each new model row, mark it NotPresent, splice into the
    // parent snapshot at the correct position, then refresh the
    // parent pair if it was matched and re-apply both snapshots.
    void recomputeAfterRowsInserted(const QModelIndex &parent,
                                    int first, int last, bool onLeft);

    // Phase A - edit-in-diff state.
    bool                       mDiffEditable = false;
    // Re-entrancy guard for the rowsInserted snapshot hook. Set true
    // while pushFromTo / pushInsertFromTo do their own snapshot
    // bookkeeping (insertPeer / copyPeer) - without this flag the
    // model-side appendChildFromJson / replaceFromJson would trigger
    // onLeftRowsInserted / onRightRowsInserted and double-splice
    // (or splice a NotPresent placeholder that fights the
    // cross-linked Identical state insertPeer just set up).
    bool                       mSuppressInsertHook = false;
    // Paths captured in rowsAboutToBeRemoved (model still has rows),
    // consumed in rowsRemoved (model gone) for the snapshot update.
    QList<QList<int>>          mPendingRemovedPathsLeft;
    QList<QList<int>>          mPendingRemovedPathsRight;
    QAction                   *mLeftPushAction    = nullptr;   // in left toolbar
    QAction                   *mRightPushAction   = nullptr;   // in right toolbar
    QAction                   *mLeftDeleteAction  = nullptr;   // Phase B
    QAction                   *mRightDeleteAction = nullptr;   // Phase B
    QSharedPointer<DiffNode>   mLeftSnap;       // kept alive after a compare
    QSharedPointer<DiffNode>   mRightSnap;      // for targeted recompare on edit
    JsonDiffEngine::Mode       mLastMode = JsonDiffEngine::Mode::FullPath;

    void setupPushButtons();
    void updatePushAction(QTreeView *view, QAction *action, QJsonModel *model,
                         DiffNode *srcSnapRoot);
    void updateDeleteAction(QTreeView *view, QAction *action);
    static QList<int> indexPath(const QModelIndex &idx);
    bool pushFromTo(QJsonModel *srcModel, const QModelIndex &srcIdx,
                    QJsonModel *dstModel, const QModelIndex &dstIdx,
                    DiffNode *srcSnapRoot, DiffNode *dstSnapRoot);
    // Phase B variants. pushInsertFromTo handles the NotPresent path:
    // walks up the source snapshot to find the matched parent on the
    // destination side, calls JsonDiffEngine::insertPeer + the model's
    // appendChildFromJson. deleteOn handles single-side removal +
    // peer-orphan + recompare.
    bool pushInsertFromTo(QJsonModel *srcModel, const QModelIndex &srcIdx,
                          QJsonModel *dstModel,
                          DiffNode *srcSnapRoot, DiffNode *dstSnapRoot);
    bool deleteOn(QJsonModel *srcModel, const QModelIndex &srcIdx,
                  QJsonModel *otherModel,
                  DiffNode *srcSnapRoot, DiffNode *otherSnapRoot);
};

#endif // QJSONDIFF_H
