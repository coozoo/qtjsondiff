/* Author: Yuriy Kuzin
 * Description: Diff like widget for JSON
 *              side by side comparison of two models
 */
#include "qjsondiff.h"
#include <functional>
#include <QGroupBox>
#include <QTime>
#include <QThread>
#include <QProgressDialog>
#include <QAction>
#include <QToolBar>
#include <QItemSelectionModel>
#include <QStyle>
#include <QApplication>
#include "qjsonitem.h"
#include "jsondiffengine.h"
#include "preferences/preferences.h"

QJsonDiff::QJsonDiff(QWidget *parent):
    QWidget(parent)
{

    prevLeftScroll=0;
    prevRightScroll=0;

    qjsoncontainer_layout = new QGridLayout();
    qDebug()<<"$container_groupbox=new QGroupBox(parent)";

    container_left_groupbox=new QGroupBox(parent);
    container_left_groupbox->setContentsMargins(QMargins(0,0,0,0));

    qDebug()<<"$cont1=new QJsonContainer(container_groupbox)";
    left_cont=new QJsonContainer(container_left_groupbox);

    container_right_groupbox=new QGroupBox(parent);
    container_right_groupbox->setContentsMargins(QMargins(0,0,0,0));

    qDebug()<<"$cont2=new QJsonContainer(container_groupbox1)";
    right_cont=new QJsonContainer(container_right_groupbox);

    left_cont->showGoto(true);
    right_cont->showGoto(true);

    showJsonButtonPosition();

    button_groupbox=new QGroupBox(parent);
    button_groupbox->setContentsMargins(QMargins(0,0,0,0));
    button_groupbox->setStyleSheet("QGroupBox{border:0;}");
    button_layout=new QGridLayout();
    compare_groupbox=new QGroupBox(parent);
    compare_groupbox->setContentsMargins(QMargins(0,0,0,0));
    compare_groupbox->setStyleSheet("QGroupBox{border:0;}");

    compare_layout=new QGridLayout();

    compare_layout->addWidget(container_left_groupbox,0,0,1,1);
    compare_layout->addWidget(container_right_groupbox,0,1,1,1);

    compare_groupbox->setLayout(compare_layout);

    qjsoncontainer_layout->addWidget(compare_groupbox,1,0);

    common_groupbox=new QGroupBox(parent);
    common_groupbox->setLayout(qjsoncontainer_layout);
    common_groupbox->setContentsMargins(QMargins(0,0,0,0));
    common_layout=new QVBoxLayout();
    parent->setLayout(common_layout);

    compare_pushbutton=new QPushButton(button_groupbox);
    compare_pushbutton->setText(tr("Compare"));
    compare_pushbutton->setToolTip(tr("Start comparison (ALT+C)"));
    compare_shortcut = new QShortcut(QKeySequence("ALT+C"), compare_pushbutton);
    button_layout->addWidget(compare_pushbutton,0,0,1,1);
    syncScroll_checkbox=new QCheckBox(button_groupbox);
    syncScroll_checkbox->setText(tr("Sync Scrolls"));
    syncScroll_checkbox->setToolTip(tr("Try to sync left and right scrolling areas"));
    button_layout->addWidget(syncScroll_checkbox,0,1);
    useFullPath_checkbox=new QCheckBox(button_groupbox);
    useFullPath_checkbox->setText(tr("Use Full Path"));
    useFullPath_checkbox->setChecked(true);
    useFullPath_checkbox->setToolTip(tr("Otherwise try to find child+parent pair anywhere in JSON tree"));
    button_layout->addWidget(useFullPath_checkbox,0,2);
    checkboxSpacer=new QSpacerItem(5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
    button_layout->addItem(checkboxSpacer,0,0,1,1);
    button_groupbox->setLayout(button_layout);
    common_layout->addWidget(button_groupbox);
    common_layout->addWidget(common_groupbox);

    compare_groupbox->layout()->setContentsMargins(QMargins(0,0,0,0));
    common_groupbox->layout()->setContentsMargins(QMargins(0,0,0,0));
    button_groupbox->layout()->setContentsMargins(QMargins(0,0,0,0));

    connect(compare_pushbutton,SIGNAL(clicked()),this,SLOT(on_compare_pushbutton_clicked()));
    connect(compare_shortcut,SIGNAL(activated()),this,SLOT(on_compare_pushbutton_clicked()));
    connect(left_cont->getTreeView(),SIGNAL(clicked(QModelIndex)),this,SLOT(on_lefttreeview_clicked(QModelIndex)));
    connect(right_cont->getTreeView(),SIGNAL(clicked(QModelIndex)),this,SLOT(on_righttreeview_clicked(QModelIndex)));
    connect(right_cont,&QJsonContainer::sJsonFileLoaded,this,&QJsonDiff::reinitLeftModel);
    connect(left_cont,&QJsonContainer::sJsonFileLoaded,this,&QJsonDiff::reinitRightModel);
    connect(right_cont,SIGNAL(jsonUpdated()),this,SLOT(reinitLeftModel()));
    connect(left_cont,SIGNAL(jsonUpdated()),this,SLOT(reinitRightModel()));
    connect(useFullPath_checkbox,&QCheckBox::stateChanged,this,&QJsonDiff::on_useFullPath_checkbox_stateChanged);
    /*//good but not enough for this case
    connect(left_cont->getTreeView()->verticalScrollBar(), SIGNAL(valueChanged(int)),
            right_cont->getTreeView()->verticalScrollBar(), SLOT(setValue(int)));
    connect(right_cont->getTreeView()->verticalScrollBar(), SIGNAL(valueChanged(int)),
            left_cont->getTreeView()->verticalScrollBar(), SLOT(setValue(int)));
            */
    connect(left_cont->getTreeView()->verticalScrollBar(),SIGNAL(valueChanged(int)),
            this,SLOT(on_lefttreeview_scroll_valuechanged(int)));
    connect(right_cont->getTreeView()->verticalScrollBar(),SIGNAL(valueChanged(int)),
            this,SLOT(on_righttreeview_scroll_valuechanged(int)));
    connect(this,&QJsonDiff::sLoadLeftJsonFile,left_cont,&QJsonContainer::loadJsonFile);
    connect(this,&QJsonDiff::sLoadRightJsonFile,right_cont,&QJsonContainer::loadJsonFile);
    connect(right_cont,&QJsonContainer::sJsonFileLoaded,this,&QJsonDiff::rightJsonFileLoaded);
    connect(left_cont,&QJsonContainer::sJsonFileLoaded,this,&QJsonDiff::leftJsonFileLoaded);

    connect(left_cont,&QJsonContainer::diffSelected,this,&QJsonDiff::on_lefttreeview_clicked);
    connect(right_cont,&QJsonContainer::diffSelected,this,&QJsonDiff::on_righttreeview_clicked);

    setupCompareWorker();
    setupPushButtons();

    // Inline-edit live diff refresh. Gated inside the slot on snapshot
    // presence — pre-Compare edits are ignored. apply() bypasses the
    // model's setData() path and only fires layoutChanged, so there's
    // no dataChanged feedback loop from these handlers.
    connect(left_cont->getJsonModel(),  &QJsonModel::dataChanged,
            this, &QJsonDiff::onLeftModelDataChanged);
    connect(right_cont->getJsonModel(), &QJsonModel::dataChanged,
            this, &QJsonDiff::onRightModelDataChanged);

    // Structural inserts (item DnD between sides, context-menu Add
    // child) — keep the snapshot in sync so the next diff interaction
    // sees the new node. The slot marks the inserted subtree gray
    // (NotPresent) and bumps the drop parent's pair color if needed.
    connect(left_cont->getJsonModel(),  &QJsonModel::rowsInserted,
            this, &QJsonDiff::onLeftRowsInserted);
    connect(right_cont->getJsonModel(), &QJsonModel::rowsInserted,
            this, &QJsonDiff::onRightRowsInserted);

    // Structural removes (context-menu Delete, toolbar arrow, any
    // future path). Clears cross-side idxRelations BEFORE the items
    // are destroyed, then updates the snapshot in rowsRemoved.
    connect(left_cont->getJsonModel(),  &QAbstractItemModel::rowsAboutToBeRemoved,
            this, &QJsonDiff::onLeftRowsAboutToBeRemoved);
    connect(right_cont->getJsonModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
            this, &QJsonDiff::onRightRowsAboutToBeRemoved);
    connect(left_cont->getJsonModel(),  &QAbstractItemModel::rowsRemoved,
            this, &QJsonDiff::onLeftRowsRemoved);
    connect(right_cont->getJsonModel(), &QAbstractItemModel::rowsRemoved,
            this, &QJsonDiff::onRightRowsRemoved);

    // When either side gets a fresh document (loadJson / paste / browse /
    // URL all go through beginResetModel/endResetModel), the cached diff
    // snapshots no longer correspond to model state. Drop them both so
    // subsequent edits don't paint stale colors / idxRelations through
    // apply(), and the next Compare rebuilds from scratch.
    auto onSideReset = [this]()
    {
        mLeftSnap.reset();
        mRightSnap.reset();
        updateLeftPushAction();
        updateRightPushAction();
        updateLeftDeleteAction();
        updateRightDeleteAction();
    };
    connect(left_cont->getJsonModel(),  &QAbstractItemModel::modelReset,
            this, onSideReset);
    connect(right_cont->getJsonModel(), &QAbstractItemModel::modelReset,
            this, onSideReset);
}

QJsonDiff::~QJsonDiff()
{
    teardownCompareWorker();
    // All the widgets and layouts constructed here are parented to
    // either `this`, the parent widget passed to the constructor, or
    // to inner group-boxes. Qt's normal parent-child cleanup destroys
    // them automatically when their owners go away. The old pattern of
    // explicit deleteLater() calls on objects we don't own crashed at
    // app exit because some of those owners (notably the parent
    // widget's own layout, common_layout) are destroyed in the parent
    // widget's ~QWidget body BEFORE ~QObject::deleteChildren reaches
    // this QJsonDiff — leaving us holding dangling pointers.
}

void QJsonDiff::reinitLeftModel()
{
    left_cont->reInitModel();
    right_cont->reInitModel();
}
void QJsonDiff::reinitRightModel()
{
    right_cont->reInitModel();
    left_cont->reInitModel();
}

void QJsonDiff::on_lefttreeview_scroll_valuechanged(int val)
{
    right_cont->getTreeView()->verticalScrollBar()->blockSignals(true);
    if(syncScroll_checkbox->isChecked())
        {
            if(val>=prevLeftScroll && val>0)
                {

                    right_cont->getTreeView()->verticalScrollBar()->setValue(right_cont->getTreeView()->verticalScrollBar()->value()+val-prevLeftScroll);
                    //little buggy version
                    //right_cont->getTreeView()->verticalScrollBar()->setValue(right_cont->getTreeView()->verticalScrollBar()->value()+left_cont->getTreeView()->verticalScrollBar()->singleStep());
                }
            else
                {
                    right_cont->getTreeView()->verticalScrollBar()->setValue(right_cont->getTreeView()->verticalScrollBar()->value()-(prevLeftScroll-val));
                }

        }
    prevLeftScroll=left_cont->getTreeView()->verticalScrollBar()->value();
    prevRightScroll=right_cont->getTreeView()->verticalScrollBar()->value();
    // update() queues a paint event. repaint() runs synchronously and
    // crashes if called while another paint is in progress on this view.
    right_cont->getTreeView()->viewport()->update();
    right_cont->getTreeView()->verticalScrollBar()->blockSignals(false);

}

void QJsonDiff::on_righttreeview_scroll_valuechanged(int val)
{
    left_cont->getTreeView()->verticalScrollBar()->blockSignals(true);
    if(syncScroll_checkbox->isChecked())
        {
            if(val>=prevRightScroll  && val>0)
                {
                    left_cont->getTreeView()->verticalScrollBar()->setValue(left_cont->getTreeView()->verticalScrollBar()->value()+val-prevLeftScroll);
                }
            else
                {
                    left_cont->getTreeView()->verticalScrollBar()->setValue(left_cont->getTreeView()->verticalScrollBar()->value()-(prevLeftScroll-val));
                }

        }
    prevLeftScroll=left_cont->getTreeView()->verticalScrollBar()->value();
    prevRightScroll=right_cont->getTreeView()->verticalScrollBar()->value();
    left_cont->getTreeView()->viewport()->update();
    left_cont->getTreeView()->verticalScrollBar()->blockSignals(false);
}


void QJsonDiff::loadJsonLeft(QJsonDocument data)
{
    left_cont->loadJson(data);
}

void QJsonDiff::loadJsonLeft(const QString &data)
{
    left_cont->loadJson(data);
}

void QJsonDiff::loadJsonRight(QJsonDocument data)
{
    right_cont->loadJson(data);
}

void QJsonDiff::loadJsonRight(const QString& data)
{
    right_cont->loadJson(data);
}
void QJsonDiff::expandIt()
{
    left_cont->expandIt();
    right_cont->expandIt();
}

QTreeView* QJsonDiff::getLeftTreeView()
{
    return left_cont->getTreeView();
}

QTreeView* QJsonDiff::getRightTreeView()
{
    return right_cont->getTreeView();
}
QJsonModel* QJsonDiff::getLeftJsonModel()
{
    return left_cont->getJsonModel();
}
QJsonModel *QJsonDiff::getRightJsonModel()
{
    return right_cont->getJsonModel();
}

void QJsonDiff::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(QPen(Qt::red,1,Qt::SolidLine));

    //p.drawLine(0,0,width(),height());
}

void QJsonDiff::setupCompareWorker()
{
    mWorkerThread = new QThread(this);
    mEngine = new JsonDiffEngine();   // no parent; deleted explicitly in teardown
    mEngine->moveToThread(mWorkerThread);

    connect(mEngine, &JsonDiffEngine::finished,
            this,    &QJsonDiff::onCompareFinished,   Qt::QueuedConnection);
    connect(mEngine, &JsonDiffEngine::cancelled,
            this,    &QJsonDiff::onCompareCancelled,  Qt::QueuedConnection);
    connect(mEngine, &JsonDiffEngine::progressed,
            this,    &QJsonDiff::onCompareProgressed, Qt::QueuedConnection);
    connect(mEngine, &JsonDiffEngine::phaseChanged,
            this, [this](JsonDiffEngine::Phase phase)
    {
        // tr() wraps each branch so lupdate picks them up — engine
        // stays UI-/translation-agnostic.
        switch (phase)
        {
        case JsonDiffEngine::Phase::BuildingPaths:
            mCurrentPhase = tr("Building paths"); break;
        case JsonDiffEngine::Phase::MatchingPaths:
            mCurrentPhase = tr("Matching paths"); break;
        case JsonDiffEngine::Phase::ComparingValues:
            mCurrentPhase = tr("Comparing values"); break;
        case JsonDiffEngine::Phase::IndexingRightTree:
            mCurrentPhase = tr("Indexing right tree"); break;
        case JsonDiffEngine::Phase::PairingItems:
            mCurrentPhase = tr("Pairing items"); break;
        case JsonDiffEngine::Phase::ResolvingColors:
            mCurrentPhase = tr("Resolving colors"); break;
        }
        if (mProgressDialog)
            mProgressDialog->setLabelText(formatProgressLabel());
    }, Qt::QueuedConnection);

    // Note: we deliberately do NOT connect QThread::finished to
    // engine.deleteLater. The worker's event loop has stopped by the
    // time that signal fires, so the deferred-delete event never gets
    // processed and the engine would leak. Engine lifetime is managed
    // explicitly by teardownCompareWorker.

    mWorkerThread->start();
}

void QJsonDiff::teardownCompareWorker()
{
    if (mProgressDialog)
    {
        mProgressDialog->hide();
        // mProgressDialog has `this` as parent; QObject's child cleanup
        // will destroy it when ~QJsonDiff finishes. Calling deleteLater
        // at shutdown is unsafe — the event loop is dying and the
        // posted DeferredDelete event has nowhere to land.
        mProgressDialog = nullptr;
    }
    if (mEngine)
    {
        mEngine->requestCancel();
    }
    if (mWorkerThread)
    {
        mWorkerThread->quit();
        mWorkerThread->wait();
        // Worker is fully stopped — no thread can be touching the
        // engine. We deliberately do NOT moveToThread() back to the
        // main thread here: Qt requires moveToThread be invoked from
        // the object's *own* current thread, and the worker that owns
        // the engine is already dead. Just delete it from main; the
        // engine has no per-thread state of its own to worry about.
        if (mEngine)
        {
            delete mEngine;
            mEngine = nullptr;
        }
        // mWorkerThread is a child of `this`. Don't `delete` it here —
        // let QObject's child cleanup destroy it when ~QJsonDiff
        // finishes (avoids mutating this widget's children list
        // mid-destruction).
        mWorkerThread = nullptr;
    }
}

void QJsonDiff::on_compare_pushbutton_clicked()
{

    // Re-entrancy gate: rapid re-click (or ALT+C re-fire) during the
    // setValue(0) event-pump below would otherwise queue a second
    // compareAsync and leave the progress dialog desynchronised from
    // the worker.
    mComparing = true;
    compare_pushbutton->setEnabled(false);

    left_cont->reInitModel();
    right_cont->reInitModel();

    QJsonModel *leftModel  = left_cont->getJsonModel();
    QJsonModel *rightModel = right_cont->getJsonModel();

    // Heap-allocate the snapshots so only the shared-pointer handles
    // cross the worker-thread boundary — see compareAsync's payload
    // contract in jsondiffengine.h.
    auto leftSnap  = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(leftModel));
    auto rightSnap = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(rightModel));

    const JsonDiffEngine::Mode mode = useFullPath_checkbox->isChecked()
        ? JsonDiffEngine::Mode::FullPath
        : JsonDiffEngine::Mode::ParentChildPair;
    mLastMode = mode;

    // Modal progress dialog blocks user input until compare completes
    // or is cancelled. We deliberately CREATE A FRESH DIALOG each compare
    // — reusing one via reset() + setValue(0) hits QProgressDialog's
    // internal state machine (shown_once flag, forceTimer interaction)
    // and can leave the widget painted-but-not-modal when the worker
    // emits finished() before the show event finishes processing. Fresh
    // allocation avoids that entire failure mode at zero meaningful cost.
    mCompareCancelledByUser = false;
    if (mProgressDialog)
    {
        mProgressDialog->hide();
        mProgressDialog->deleteLater();
        mProgressDialog = nullptr;
    }
    mProgressDialog = new QProgressDialog(this);
    mProgressDialog->setWindowTitle(tr("Comparing JSON"));
    mProgressDialog->setCancelButtonText(tr("Cancel"));
    mProgressDialog->setWindowModality(Qt::ApplicationModal);
    mProgressDialog->setAutoClose(false);
    mProgressDialog->setAutoReset(false);
    mProgressDialog->setMinimumDuration(0);
    mProgressDialog->setRange(0, mode == JsonDiffEngine::Mode::FullPath ? 6 : 3);
    connect(mProgressDialog, &QProgressDialog::canceled, this, [this]()
    {
        mCompareCancelledByUser = true;
        if (mEngine) mEngine->requestCancel();
    });
    mCurrentPhase = tr("Starting");
    mCompareElapsed.start();
    mProgressDialog->setLabelText(formatProgressLabel());
    mProgressDialog->show();

    mEngine->resetCancel();
    QMetaObject::invokeMethod(mEngine, "compareAsync", Qt::QueuedConnection,
        Q_ARG(QSharedPointer<DiffNode>, leftSnap),
        Q_ARG(QSharedPointer<DiffNode>, rightSnap),
        Q_ARG(JsonDiffEngine::Mode, mode));
}

void QJsonDiff::onCompareFinished(QSharedPointer<DiffNode> left,
                                  QSharedPointer<DiffNode> right)
{
    if (mProgressDialog)
    {
        mProgressDialog->hide();
        mProgressDialog->deleteLater();
        mProgressDialog = nullptr;
    }

    // Release the re-entrancy gate and the visual lock on the Compare
    // button. Done before any of the early-returns below so the user
    // can re-Compare even when the result was discarded.
    mComparing = false;
    compare_pushbutton->setEnabled(true);

    // If the user clicked Cancel just as the engine emitted finished()
    // (race window), honor the user's intent and discard the result.
    if (mCompareCancelledByUser)
        return;

    if (!left || !right)
        return;

    QJsonModel *leftModel  = left_cont->getJsonModel();
    QJsonModel *rightModel = right_cont->getJsonModel();
    JsonDiffEngine::apply(*left,  leftModel,  rightModel);
    JsonDiffEngine::apply(*right, rightModel, leftModel);

    // Keep the snapshots alive — Phase A targeted recompare runs on
    // these without ever rebuilding from the model.
    mLeftSnap  = left;
    mRightSnap = right;

    left_cont->gotoIndexHandler(true);
    right_cont->gotoIndexHandler(true);

    updateLeftPushAction();
    updateRightPushAction();
}

void QJsonDiff::onCompareCancelled()
{
    if (mProgressDialog)
    {
        mProgressDialog->hide();
        mProgressDialog->deleteLater();
        mProgressDialog = nullptr;
    }
    mComparing = false;
    compare_pushbutton->setEnabled(true);
    // Discard partial — no apply (Phase 2 design Q3).
}

QString QJsonDiff::formatProgressLabel() const
{
    const QString phase = mCurrentPhase.isEmpty()
        ? tr("Running comparison")
        : mCurrentPhase;
    if (!mCompareElapsed.isValid())
        return phase + QStringLiteral("…");
    const qint64 secs = mCompareElapsed.elapsed() / 1000;
    return tr("%1… (%2s)").arg(phase).arg(secs);
}

void QJsonDiff::onCompareProgressed(int done, int total)
{
    // setValue() pumps the event loop on a modal dialog. Inside that
    // pump the queued finished() can drain — onCompareFinished then
    // deleteLater()s our dialog and the QPointer auto-nulls. Re-check
    // after every dialog call so we don't touch a destroyed widget.
    if (!mProgressDialog) return;
    mProgressDialog->setRange(0, total);
    if (!mProgressDialog) return;
    mProgressDialog->setValue(done);
    if (!mProgressDialog) return;
    mProgressDialog->setLabelText(formatProgressLabel());
}

void QJsonDiff::startComparison()
{
    QJsonModel *leftModel  = left_cont->getJsonModel();
    QJsonModel *rightModel = right_cont->getJsonModel();

    // Synchronous compare path. Used by integrators that don't want the
    // built-in progress dialog and by the unit tests. The Compare button
    // uses the threaded path with a modal progress dialog instead — see
    // on_compare_pushbutton_clicked().
    DiffNode leftSnap  = JsonDiffEngine::snapshot(leftModel);
    DiffNode rightSnap = JsonDiffEngine::snapshot(rightModel);

    const JsonDiffEngine::Mode mode = useFullPath_checkbox->isChecked()
        ? JsonDiffEngine::Mode::FullPath
        : JsonDiffEngine::Mode::ParentChildPair;
    mLastMode = mode;

    JsonDiffEngine::compare(leftSnap, rightSnap, mode);

    JsonDiffEngine::apply(leftSnap,  leftModel,  rightModel);
    JsonDiffEngine::apply(rightSnap, rightModel, leftModel);

    // Cache snapshots for incremental Phase A edits — store moved copies
    // on the heap so subsequent recomparePair calls operate on the same
    // graph the model now reflects.
    mLeftSnap  = QSharedPointer<DiffNode>::create(std::move(leftSnap));
    mRightSnap = QSharedPointer<DiffNode>::create(std::move(rightSnap));
}

void QJsonDiff::setBrowseVisible(bool state)
{
    left_cont->setBrowseVisible(state);
    right_cont->setBrowseVisible(state);
}

void QJsonDiff::on_lefttreeview_clicked(const QModelIndex&)
{
    qDebug()<<"left tree clicked";

    QTreeView *lefttreeview=left_cont->getTreeView();
    QModelIndex idx=lefttreeview->currentIndex();
    QJsonModel *leftmodel=left_cont->getJsonModel();
    QJsonTreeItem *item=leftmodel->itemFromIndex(idx);

    QTreeView *righttreeview=right_cont->getTreeView();

    if(idx.isValid())
        {
            qDebug()<<"left tree idx valid";
            qDebug()<<item->key()<<item->value()<<item->idxRelation();

            if(item->idxRelation().isValid())
                {
                    righttreeview->setCurrentIndex(item->idxRelation());
                }
        }
}

void QJsonDiff::on_righttreeview_clicked(const QModelIndex&)
{
    qDebug()<<"right tree clicked";

    QTreeView *righttreeview=right_cont->getTreeView();
    QModelIndex idx=righttreeview->currentIndex();
    QJsonModel *rightmodel=right_cont->getJsonModel();
    QJsonTreeItem *item=rightmodel->itemFromIndex(idx);

    QTreeView *lefttreeview=left_cont->getTreeView();

    if(idx.isValid())
        {
            qDebug()<<"right tree idx valid";
            qDebug()<<item->key()<<item->value()<<item->idxRelation();
            if(item->idxRelation().isValid())
                {
                    int temp=lefttreeview->horizontalScrollBar()->value();
                    lefttreeview->setCurrentIndex(item->idxRelation());
                    lefttreeview->horizontalScrollBar()->setValue(temp);
                }
        }

}


void QJsonDiff::on_useFullPath_checkbox_stateChanged(int)
{
    qDebug()<<"on_useFullPath_checkbox_checkStateChanged";
    reinitLeftModel();
}

// load json file
void QJsonDiff::loadRightJsonFile(const QString &target)
{
    qDebug()<<"loadRightJsonFile";
    emit sLoadRightJsonFile(target);
}

// load json file
void QJsonDiff::loadLeftJsonFile(const QString &target)
{
    qDebug()<<"loadLeftJsonFile";
    emit sLoadLeftJsonFile(target);
}

//emit signal whn new file or url loaded
void QJsonDiff::rightJsonFileLoaded(const QString &path)
{
    qDebug()<<"rightJsonFileLoaded";
    emit sRightJsonFileLoaded(path);
}
//emit signal whn new file or url loaded
void QJsonDiff::leftJsonFileLoaded(const QString &path)
{
    qDebug()<<"leftJsonFileLoaded";
    emit sLeftJsonFileLoaded(path);
}

void QJsonDiff::showJsonButtonPosition()
{
    qDebug()<<"showJsonButtonPosition";
    left_cont->showJsonButtonPosition();
    right_cont->showJsonButtonPosition();
}

// -----------------------------------------------------------------------
// Phase A — edit-in-diff
// -----------------------------------------------------------------------

void QJsonDiff::setDiffEditable(bool editable)
{
    mDiffEditable = editable;
    left_cont->setEditable(editable);
    right_cont->setEditable(editable);
    updateLeftPushAction();
    updateRightPushAction();
    updateLeftDeleteAction();
    updateRightDeleteAction();
}

bool QJsonDiff::isDiffEditable() const
{
    return mDiffEditable;
}

void QJsonDiff::setupPushButtons()
{
    // Each side gets one "push selected → other side" action, parked
    // in the side's existing toolbar near the diff counter / prev/next-
    // diff buttons. Always in the same place, hidden when nothing
    // meaningful is selected.
    //
    // Icons matter: QToolBar's default toolButtonStyle is IconOnly, so
    // text-only actions render as a blank cell. Use Qt's built-in
    // standard arrow pixmaps for guaranteed visibility under any style.
    QStyle *st = QApplication::style();

    mLeftPushAction = new QAction(this);
    mLeftPushAction->setIcon(st->standardIcon(QStyle::SP_ArrowRight));
    mLeftPushAction->setText(tr("Push to right"));
    mLeftPushAction->setToolTip(tr("Push selected row into the matching row on the right side (overwrites)"));
    mLeftPushAction->setVisible(false);
    left_cont->toolbar->addAction(mLeftPushAction);

    mRightPushAction = new QAction(this);
    mRightPushAction->setIcon(st->standardIcon(QStyle::SP_ArrowLeft));
    mRightPushAction->setText(tr("Push to left"));
    mRightPushAction->setToolTip(tr("Push selected row into the matching row on the left side (overwrites)"));
    mRightPushAction->setVisible(false);
    right_cont->toolbar->addAction(mRightPushAction);

    // Phase B: delete-here actions, parked next to the push actions.
    mLeftDeleteAction = new QAction(this);
    mLeftDeleteAction->setIcon(st->standardIcon(QStyle::SP_TrashIcon));
    mLeftDeleteAction->setText(tr("Delete here"));
    mLeftDeleteAction->setToolTip(tr("Remove the selected row from this side. If it had a peer on the other side, the peer becomes NotPresent."));
    mLeftDeleteAction->setVisible(false);
    left_cont->toolbar->addAction(mLeftDeleteAction);

    mRightDeleteAction = new QAction(this);
    mRightDeleteAction->setIcon(st->standardIcon(QStyle::SP_TrashIcon));
    mRightDeleteAction->setText(tr("Delete here"));
    mRightDeleteAction->setToolTip(tr("Remove the selected row from this side. If it had a peer on the other side, the peer becomes NotPresent."));
    mRightDeleteAction->setVisible(false);
    right_cont->toolbar->addAction(mRightDeleteAction);

    connect(mLeftPushAction,  &QAction::triggered,
            this, &QJsonDiff::pushLeftSelectionToRight);
    connect(mRightPushAction, &QAction::triggered,
            this, &QJsonDiff::pushRightSelectionToLeft);
    connect(mLeftDeleteAction,  &QAction::triggered,
            this, &QJsonDiff::deleteLeftSelection);
    connect(mRightDeleteAction, &QAction::triggered,
            this, &QJsonDiff::deleteRightSelection);

    QTreeView *lv = left_cont->getTreeView();
    QTreeView *rv = right_cont->getTreeView();
    connect(lv->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &QJsonDiff::updateLeftPushAction);
    connect(rv->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &QJsonDiff::updateRightPushAction);
    connect(lv->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &QJsonDiff::updateLeftDeleteAction);
    connect(rv->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &QJsonDiff::updateRightDeleteAction);
}

void QJsonDiff::updatePushAction(QTreeView *view, QAction *action, QJsonModel *model,
                                 DiffNode *srcSnapRoot)
{
    if (!action || !view || !model) return;
    // Visibility tracks edit mode. The action's slot is the user-
    // facing affordance — it shouldn't fade in and out as the cursor
    // moves between rows. Enabledness reflects whether the current
    // selection can actually drive a push.
    action->setVisible(mDiffEditable);
    if (!mDiffEditable)              { action->setEnabled(false); return; }

    QModelIndex idx = view->currentIndex();
    if (!idx.isValid())              { action->setEnabled(false); return; }
    QJsonTreeItem *item = model->itemFromIndex(idx);
    if (!item)                       { action->setEnabled(false); return; }

    const DiffColorType c = item->colorType();

    // Phase A: Huge with a valid peer → overwrite on the other side.
    if (c == DiffColorType::Huge && item->idxRelation().isValid())
    {
        action->setEnabled(true);
        return;
    }

    // Phase B: NotPresent → insert into the matched parent on the
    // other side, but only when that parent actually has a peer.
    // Without the resolve check we'd enable the arrow for chained-
    // missing rows (parent also NotPresent) where the click is a
    // confirmed no-op. Snapshot may not exist yet before the first
    // compare — gate on that too.
    if (c == DiffColorType::NotPresent && srcSnapRoot)
    {
        QList<int> dstParentPath;
        if (JsonDiffEngine::resolveDstParentPath(*srcSnapRoot,
                                                 indexPath(idx),
                                                 dstParentPath))
        {
            action->setEnabled(true);
            return;
        }
    }

    action->setEnabled(false);
}

void QJsonDiff::updateLeftPushAction()
{
    updatePushAction(left_cont->getTreeView(),
                     mLeftPushAction,
                     left_cont->getJsonModel(),
                     mLeftSnap.data());
}

void QJsonDiff::updateRightPushAction()
{
    updatePushAction(right_cont->getTreeView(),
                     mRightPushAction,
                     right_cont->getJsonModel(),
                     mRightSnap.data());
}

void QJsonDiff::updateDeleteAction(QTreeView *view, QAction *action)
{
    if (!action || !view)            { return; }
    // Visibility tracks edit mode (matches the push actions). Enabled
    // tracks whether the selection is a non-root row that the model
    // can actually remove.
    action->setVisible(mDiffEditable);
    if (!mDiffEditable)              { action->setEnabled(false); return; }
    QModelIndex idx = view->currentIndex();
    action->setEnabled(idx.isValid() && idx.parent().isValid());
}

void QJsonDiff::updateLeftDeleteAction()
{
    updateDeleteAction(left_cont->getTreeView(), mLeftDeleteAction);
}

void QJsonDiff::updateRightDeleteAction()
{
    updateDeleteAction(right_cont->getTreeView(), mRightDeleteAction);
}

QList<int> QJsonDiff::indexPath(const QModelIndex &idx)
{
    QList<int> path;
    QModelIndex cur = idx;
    while (cur.isValid())
    {
        path.prepend(cur.row());
        cur = cur.parent();
    }
    return path;
}

bool QJsonDiff::pushFromTo(QJsonModel *srcModel, const QModelIndex &srcIdx,
                           QJsonModel *dstModel, const QModelIndex &dstIdx,
                           DiffNode *srcSnapRoot, DiffNode *dstSnapRoot)
{
    if (!srcModel || !dstModel || !srcSnapRoot || !dstSnapRoot)
        return false;
    if (!srcIdx.isValid() || !dstIdx.isValid())
        return false;

    const QList<int> srcPath = indexPath(srcIdx);
    const QList<int> dstPath = indexPath(dstIdx);

    // 1. Build the JSON value to push from the cached source snapshot.
    DiffNode *srcNode = nullptr;
    {
        DiffNode *cur = srcSnapRoot;
        for (int step : srcPath)
        {
            if (step < 0 || step >= cur->children.size()) return false;
            cur = &cur->children[step];
        }
        srcNode = cur;
    }
    DiffNode *dstNode = nullptr;
    {
        DiffNode *cur = dstSnapRoot;
        for (int step : dstPath)
        {
            if (step < 0 || step >= cur->children.size()) return false;
            cur = &cur->children[step];
        }
        dstNode = cur;
    }

    const QJsonValue payload = JsonDiffEngine::toJsonValue(*srcNode);

    // 2. In-place replace on the model — emits dataChanged + dataUpdated,
    // so the container's find/goto caches reset automatically. Suppress
    // the rowsInserted hook for the duration: the snapshot bookkeeping
    // for the new children is done by copyPeer below, not by re-walking
    // model state.
    {
        const bool prev = mSuppressInsertHook;
        mSuppressInsertHook = true;
        const bool ok = dstModel->replaceFromJson(dstIdx, payload);
        mSuppressInsertHook = prev;
        if (!ok) return false;
    }

    // 3. Mirror the change on the cached snapshot and re-evaluate the
    // pair + ancestor chain. No full re-snapshot, no full re-compare.
    JsonDiffEngine::copyPeer(*dstNode, *srcNode);
    JsonDiffEngine::recomparePair(*srcSnapRoot, srcPath,
                                  *dstSnapRoot, dstPath,
                                  mLastMode);

    // 4. Push colors back onto both models so view repaints with the
    // updated highlighting.
    JsonDiffEngine::apply(*srcSnapRoot, srcModel, dstModel);
    JsonDiffEngine::apply(*dstSnapRoot, dstModel, srcModel);

    // 5. Refresh the push buttons — the row's color just flipped to
    // Identical, so the action that was visible on it should hide.
    updateLeftPushAction();
    updateRightPushAction();
    updateLeftDeleteAction();
    updateRightDeleteAction();
    return true;
}

bool QJsonDiff::pushInsertFromTo(QJsonModel *srcModel, const QModelIndex &srcIdx,
                                 QJsonModel *dstModel,
                                 DiffNode *srcSnapRoot, DiffNode *dstSnapRoot)
{
    if (!srcModel || !dstModel || !srcSnapRoot || !dstSnapRoot)
        return false;
    if (!srcIdx.isValid())
        return false;

    const QList<int> srcPath = indexPath(srcIdx);

    // Resolve the destination parent path on the OTHER snapshot via
    // the source-side parent's relationPath. If the parent isn't paired
    // (e.g. it's also NotPresent), bail — the user needs to push the
    // missing parent first.
    QList<int> dstParentPath;
    if (!JsonDiffEngine::resolveDstParentPath(*srcSnapRoot, srcPath, dstParentPath))
        return false;

    // Navigate the destination MODEL to the parent index that matches
    // dstParentPath. Empty path == invalid index == root.
    QModelIndex dstParentIdx;
    for (int step : dstParentPath)
        dstParentIdx = dstModel->index(step, 0, dstParentIdx);

    // Pull the source DiffNode for payload + key.
    DiffNode *srcNode = srcSnapRoot;
    for (int step : srcPath)
    {
        if (step < 0 || step >= srcNode->children.size()) return false;
        srcNode = &srcNode->children[step];
    }
    const QJsonValue payload = JsonDiffEngine::toJsonValue(*srcNode);

    // Append on the model. The model picks the new child's key for us
    // (array parents use the new index; object parents use `key`).
    // Suppress the rowsInserted hook: insertPeer below installs the
    // matched-pair Identical + cross-linked snapshot state, which the
    // hook would otherwise overwrite with a NotPresent placeholder.
    QModelIndex newIdx;
    {
        const bool prev = mSuppressInsertHook;
        mSuppressInsertHook = true;
        newIdx = dstModel->appendChildFromJson(dstParentIdx,
                                              srcNode->key, payload);
        mSuppressInsertHook = prev;
    }
    if (!newIdx.isValid())
        return false;

    // Mirror on the snapshot. insertPeer cross-links the whole subtree
    // and refreshes ancestors on both sides.
    JsonDiffEngine::insertPeer(*srcSnapRoot, srcPath,
                               *dstSnapRoot, dstParentPath,
                               mLastMode);

    JsonDiffEngine::apply(*srcSnapRoot, srcModel, dstModel);
    JsonDiffEngine::apply(*dstSnapRoot, dstModel, srcModel);

    updateLeftPushAction();
    updateRightPushAction();
    updateLeftDeleteAction();
    updateRightDeleteAction();
    return true;
}

bool QJsonDiff::deleteOn(QJsonModel *srcModel, const QModelIndex &srcIdx,
                         QJsonModel *otherModel,
                         DiffNode *srcSnapRoot, DiffNode *otherSnapRoot)
{
    if (!srcModel || !otherModel || !srcSnapRoot || !otherSnapRoot)
        return false;
    if (!srcIdx.isValid())
        return false;

    const QList<int> srcPath = indexPath(srcIdx);
    if (srcPath.isEmpty()) return false;            // root rejected

    // Peer-side cleanup (clear stale idxRelations + color, snapshot
    // removePeer + apply for ancestor color refresh) is driven by the
    // rowsAboutToBeRemoved hook which fires from inside removeRowAt.
    if (!srcModel->removeRowAt(srcIdx))
        return false;

    updateLeftPushAction();
    updateRightPushAction();
    updateLeftDeleteAction();
    updateRightDeleteAction();
    return true;
}

bool QJsonDiff::pushLeftSelectionToRight()
{
    if (!mLeftSnap || !mRightSnap) return false;
    QTreeView *lv = left_cont->getTreeView();
    QJsonModel *lm = left_cont->getJsonModel();
    QJsonModel *rm = right_cont->getJsonModel();
    if (!lv || !lm || !rm) return false;
    QModelIndex selIdx = lv->currentIndex();
    if (!selIdx.isValid()) return false;
    QJsonTreeItem *item = lm->itemFromIndex(selIdx);
    if (!item) return false;

    if (item->colorType() == DiffColorType::NotPresent)
    {
        return pushInsertFromTo(lm, selIdx, rm,
                                mLeftSnap.data(), mRightSnap.data());
    }
    QModelIndex peerIdx = item->idxRelation();
    if (!peerIdx.isValid()) return false;
    return pushFromTo(lm, selIdx, rm, peerIdx,
                      mLeftSnap.data(), mRightSnap.data());
}

bool QJsonDiff::pushRightSelectionToLeft()
{
    if (!mLeftSnap || !mRightSnap) return false;
    QTreeView *rv = right_cont->getTreeView();
    QJsonModel *lm = left_cont->getJsonModel();
    QJsonModel *rm = right_cont->getJsonModel();
    if (!rv || !lm || !rm) return false;
    QModelIndex selIdx = rv->currentIndex();
    if (!selIdx.isValid()) return false;
    QJsonTreeItem *item = rm->itemFromIndex(selIdx);
    if (!item) return false;

    if (item->colorType() == DiffColorType::NotPresent)
    {
        return pushInsertFromTo(rm, selIdx, lm,
                                mRightSnap.data(), mLeftSnap.data());
    }
    QModelIndex peerIdx = item->idxRelation();
    if (!peerIdx.isValid()) return false;
    return pushFromTo(rm, selIdx, lm, peerIdx,
                      mRightSnap.data(), mLeftSnap.data());
}

bool QJsonDiff::deleteLeftSelection()
{
    if (!mLeftSnap || !mRightSnap) return false;
    QTreeView *lv = left_cont->getTreeView();
    QJsonModel *lm = left_cont->getJsonModel();
    QJsonModel *rm = right_cont->getJsonModel();
    if (!lv || !lm || !rm) return false;
    return deleteOn(lm, lv->currentIndex(), rm,
                    mLeftSnap.data(), mRightSnap.data());
}

bool QJsonDiff::deleteRightSelection()
{
    if (!mLeftSnap || !mRightSnap) return false;
    QTreeView *rv = right_cont->getTreeView();
    QJsonModel *lm = left_cont->getJsonModel();
    QJsonModel *rm = right_cont->getJsonModel();
    if (!rv || !lm || !rm) return false;
    return deleteOn(rm, rv->currentIndex(), lm,
                    mRightSnap.data(), mLeftSnap.data());
}

void QJsonDiff::onLeftModelDataChanged(const QModelIndex &topLeft,
                                       const QModelIndex &bottomRight,
                                       const QList<int> &roles)
{
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);
    recomputeAfterUserEdit(topLeft, /*onLeft=*/true);
}

void QJsonDiff::onRightModelDataChanged(const QModelIndex &topLeft,
                                        const QModelIndex &bottomRight,
                                        const QList<int> &roles)
{
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);
    recomputeAfterUserEdit(topLeft, /*onLeft=*/false);
}

void QJsonDiff::onLeftRowsInserted(const QModelIndex &parent, int first, int last)
{
    recomputeAfterRowsInserted(parent, first, last, /*onLeft=*/true);
}

void QJsonDiff::onRightRowsInserted(const QModelIndex &parent, int first, int last)
{
    recomputeAfterRowsInserted(parent, first, last, /*onLeft=*/false);
}

static void clearPeerLinksRecursively(QJsonTreeItem *it, QJsonModel *peerModel)
{
    if (!it || !peerModel) return;
    QModelIndex peer = it->idxRelation();
    if (peer.isValid())
    {
        if (QJsonTreeItem *peerItem = peerModel->itemFromIndex(peer))
        {
            peerItem->setIdxRelation(QModelIndex());
            peerItem->setColorType(DiffColorType::NotPresent);
        }
    }
    for (int i = 0; i < it->childCount(); ++i)
        clearPeerLinksRecursively(it->child(i), peerModel);
}

void QJsonDiff::onLeftRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    QJsonModel *srcModel  = left_cont->getJsonModel();
    QJsonModel *peerModel = right_cont->getJsonModel();
    for (int row = first; row <= last; ++row)
    {
        QModelIndex idx = srcModel->index(row, 0, parent);
        clearPeerLinksRecursively(srcModel->itemFromIndex(idx), peerModel);
        if (mLeftSnap && mRightSnap)
            mPendingRemovedPathsLeft.append(indexPath(idx));
    }
}

void QJsonDiff::onRightRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    QJsonModel *srcModel  = right_cont->getJsonModel();
    QJsonModel *peerModel = left_cont->getJsonModel();
    for (int row = first; row <= last; ++row)
    {
        QModelIndex idx = srcModel->index(row, 0, parent);
        clearPeerLinksRecursively(srcModel->itemFromIndex(idx), peerModel);
        if (mLeftSnap && mRightSnap)
            mPendingRemovedPathsRight.append(indexPath(idx));
    }
}

void QJsonDiff::onLeftRowsRemoved(const QModelIndex &, int, int)
{
    if (!mLeftSnap || !mRightSnap) { mPendingRemovedPathsLeft.clear(); return; }
    // Process descending to avoid index-shift issues within the same parent.
    std::sort(mPendingRemovedPathsLeft.begin(), mPendingRemovedPathsLeft.end(),
              std::greater<QList<int>>());
    for (const QList<int> &p : mPendingRemovedPathsLeft)
        if (!p.isEmpty())
            JsonDiffEngine::removePeer(*mLeftSnap, p, *mRightSnap, mLastMode);
    mPendingRemovedPathsLeft.clear();
    JsonDiffEngine::apply(*mLeftSnap,  left_cont->getJsonModel(),  right_cont->getJsonModel());
    JsonDiffEngine::apply(*mRightSnap, right_cont->getJsonModel(), left_cont->getJsonModel());
    updateLeftPushAction();  updateRightPushAction();
    updateLeftDeleteAction(); updateRightDeleteAction();
}

void QJsonDiff::onRightRowsRemoved(const QModelIndex &, int, int)
{
    if (!mLeftSnap || !mRightSnap) { mPendingRemovedPathsRight.clear(); return; }
    std::sort(mPendingRemovedPathsRight.begin(), mPendingRemovedPathsRight.end(),
              std::greater<QList<int>>());
    for (const QList<int> &p : mPendingRemovedPathsRight)
        if (!p.isEmpty())
            JsonDiffEngine::removePeer(*mRightSnap, p, *mLeftSnap, mLastMode);
    mPendingRemovedPathsRight.clear();
    JsonDiffEngine::apply(*mLeftSnap,  left_cont->getJsonModel(),  right_cont->getJsonModel());
    JsonDiffEngine::apply(*mRightSnap, right_cont->getJsonModel(), left_cont->getJsonModel());
    updateLeftPushAction();  updateRightPushAction();
    updateLeftDeleteAction(); updateRightDeleteAction();
}

void QJsonDiff::recomputeAfterRowsInserted(const QModelIndex &parent,
                                           int first, int last, bool onLeft)
{
    if (mSuppressInsertHook) return;          // engine-driven insert
    if (!mLeftSnap || !mRightSnap) return;   // pre-Compare → nothing to do
    if (first < 0 || last < first) return;

    DiffNode *mySnap   = onLeft ? mLeftSnap.data()  : mRightSnap.data();
    DiffNode *peerSnap = onLeft ? mRightSnap.data() : mLeftSnap.data();
    QJsonModel *myModel   = onLeft ? left_cont->getJsonModel()
                                   : right_cont->getJsonModel();
    QJsonModel *peerModel = onLeft ? right_cont->getJsonModel()
                                   : left_cont->getJsonModel();

    // Path to the drop parent (empty list = root).
    QList<int> parentPath;
    if (parent.isValid())
        parentPath = indexPath(parent.siblingAtColumn(0));

    // Navigate the snapshot to the same node.
    DiffNode *parentSnap = mySnap;
    for (int step : parentPath)
    {
        if (step < 0 || step >= parentSnap->children.size()) return;
        parentSnap = &parentSnap->children[step];
    }

    // Build and splice in a fresh DiffNode for each newly-inserted row,
    // marking the whole subtree NotPresent (it has no peer on the other
    // side). Walking [first..last] in order keeps row indices stable as
    // we insert into parentSnap->children.
    std::function<void(DiffNode&)> markAllNotPresent = [&](DiffNode &n)
    {
        n.color = DiffColorType::NotPresent;
        n.relationPath.clear();
        for (DiffNode &c : n.children) markAllNotPresent(c);
    };

    for (int row = first; row <= last; ++row)
    {
        QModelIndex childIdx = myModel->index(row, 0, parent);
        if (!childIdx.isValid()) return;
        DiffNode newNode = JsonDiffEngine::snapshotIndex(myModel, childIdx);
        markAllNotPresent(newNode);
        // Clamp the insert position — defensive against any future
        // model that emits rowsInserted with a row past childCount.
        const int insertAt = qMin(row, int(parentSnap->children.size()));
        parentSnap->children.insert(insertAt, newNode);
    }

    // Refresh the drop parent's pair color if it was matched.
    // Child counts diverged → pair likely flips to Huge, and ancestors
    // up to either root get their Moderate state recomputed.
    // Root case (parentPath empty) skips this: the root has no peer
    // pairing in the same sense, and there are no ancestors to fix.
    if (!parentPath.isEmpty() && !parentSnap->relationPath.isEmpty())
    {
        const QList<int> peerPath = parentSnap->relationPath;
        if (onLeft)
            JsonDiffEngine::recomparePair(*mySnap, parentPath,
                                          *peerSnap, peerPath,
                                          mLastMode);
        else
            JsonDiffEngine::recomparePair(*peerSnap, peerPath,
                                          *mySnap, parentPath,
                                          mLastMode);
    }

    JsonDiffEngine::apply(*mySnap,   myModel,   peerModel);
    JsonDiffEngine::apply(*peerSnap, peerModel, myModel);
}

void QJsonDiff::recomputeAfterUserEdit(const QModelIndex &idx, bool onLeft)
{
    if (!mLeftSnap || !mRightSnap) return;   // pre-Compare → nothing to do
    if (!idx.isValid()) return;

    DiffNode *mySnap   = onLeft ? mLeftSnap.data()  : mRightSnap.data();
    DiffNode *peerSnap = onLeft ? mRightSnap.data() : mLeftSnap.data();
    QJsonModel *myModel   = onLeft ? left_cont->getJsonModel()
                                   : right_cont->getJsonModel();
    QJsonModel *peerModel = onLeft ? right_cont->getJsonModel()
                                   : left_cont->getJsonModel();

    // Path uses column-0 sibling so it matches snapshot navigation.
    const QModelIndex rowIdx = idx.siblingAtColumn(0);
    const QList<int> myPath = indexPath(rowIdx);
    if (myPath.isEmpty()) return;            // root has no diff state

    // Navigate to the snapshot node.
    DiffNode *myNode = mySnap;
    for (int step : myPath)
    {
        if (step < 0 || step >= myNode->children.size()) return;
        myNode = &myNode->children[step];
    }
    QJsonTreeItem *item = myModel->itemFromIndex(rowIdx);
    if (!item) return;

    const QString modelKey   = item->key();
    const QJsonValue::Type modelType = item->type();
    const QString modelValue = item->value();

    const bool keyChanged   = (myNode->key   != modelKey);
    const bool typeChanged  = (myNode->type  != modelType);
    const bool valueChanged = (myNode->value != modelValue);
    if (!keyChanged && !typeChanged && !valueChanged) return;  // engine path already in sync

    // Sync snapshot scalar fields to model. Children get re-walked
    // below if the type changed — Object↔Array conversion paths
    // restructure the model's children list and the snapshot has to
    // catch up so future incremental edits keep working.
    myNode->key   = modelKey;
    myNode->type  = modelType;
    myNode->value = modelValue;

    // Track the peer path BEFORE the recompare/detach so we can apply
    // a targeted refresh on the peer side without walking the whole
    // peer tree. Value/key edits don't touch structure, so applyPath
    // is sufficient; type edits restructure children so they fall back
    // to the full apply.
    const QList<int> peerPathAtEdit = myNode->relationPath;

    if (keyChanged)
    {
        // The renamed slot is no longer "the same item" as before, so
        // detach from the peer (both sides go NotPresent). User can
        // still edit data freely; a fresh Compare re-pairs.
        JsonDiffEngine::detachPair(*mySnap, myPath, *peerSnap, mLastMode);
    }
    else
    {
        if (typeChanged)
        {
            // Children may have been added/removed by the model's
            // type-conversion code. Re-walk so the snapshot subtree
            // matches the model; any peer cross-link pointing INTO
            // the old subtree gets orphaned.
            JsonDiffEngine::resnapshotSubtree(*mySnap, myPath, myModel,
                                              *peerSnap);
        }
        // Type or value drift: same identity, different content →
        // recomparePair lights up Huge on the pair and refreshes
        // ancestor Moderate state on both sides.
        if (!peerPathAtEdit.isEmpty())
        {
            if (onLeft)
                JsonDiffEngine::recomparePair(*mySnap, myPath,
                                              *peerSnap, peerPathAtEdit,
                                              mLastMode);
            else
                JsonDiffEngine::recomparePair(*peerSnap, peerPathAtEdit,
                                              *mySnap, myPath,
                                              mLastMode);
        }
    }

    // Type change restructured the subtree — the full apply is needed
    // so views pick up the new/removed child rows. Value edits and key
    // renames only shift color+idxRelation along the edited path and
    // its ancestors, so the targeted applyPath saves the layoutChanged
    // cost that was making per-edit repaints hitch on ~10k-node trees.
    if (typeChanged)
    {
        JsonDiffEngine::apply(*mySnap,   myModel,   peerModel);
        JsonDiffEngine::apply(*peerSnap, peerModel, myModel);
    }
    else
    {
        JsonDiffEngine::applyPath(*mySnap, myModel, myPath, peerModel);
        if (!peerPathAtEdit.isEmpty())
            JsonDiffEngine::applyPath(*peerSnap, peerModel,
                                      peerPathAtEdit, myModel);
        else if (keyChanged)
        {
            // Rename detached the pair — the peer went NotPresent too.
            // Its color/idxRelation changed but we don't have its path
            // handy any more (detachPair cleared it). Full apply on
            // the peer keeps behaviour identical to the pre-fix path;
            // only fires on rename, which is rare and cheap enough.
            JsonDiffEngine::apply(*peerSnap, peerModel, myModel);
        }
    }
}


