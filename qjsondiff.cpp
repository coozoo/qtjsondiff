/* Author: Yuriy Kuzin
 * Description: Diff like widget for JSON
 *              side by side comparison of two models
 */
#include "qjsondiff.h"
#include <QGroupBox>
#include <QTime>
#include <QThread>
#include <QProgressDialog>
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
    //right_cont->getTreeView()->repaint();
    right_cont->getTreeView()->viewport()->repaint();
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
    //left_cont->getTreeView()->repaint();
    left_cont->getTreeView()->viewport()->repaint();
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
    qDebug()<<"compare button clicked";

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

    // Modal progress dialog blocks user input until compare completes
    // or is cancelled. Lives until either finished/cancelled fires.
    mCompareCancelledByUser = false;
    if (!mProgressDialog)
    {
        mProgressDialog = new QProgressDialog(this);
        mProgressDialog->setWindowTitle(tr("Comparing JSON"));
        mProgressDialog->setLabelText(tr("Running comparison..."));
        mProgressDialog->setCancelButtonText(tr("Cancel"));
        mProgressDialog->setWindowModality(Qt::ApplicationModal);
        mProgressDialog->setAutoClose(false);
        mProgressDialog->setAutoReset(false);
        mProgressDialog->setMinimumDuration(0);
        connect(mProgressDialog, &QProgressDialog::canceled, this, [this]()
        {
            mCompareCancelledByUser = true;
            if (mEngine) mEngine->requestCancel();
        });
    }
    mProgressDialog->setRange(0, mode == JsonDiffEngine::Mode::FullPath ? 6 : 3);
    mProgressDialog->setValue(0);
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
        mProgressDialog->hide();

    // If the user clicked Cancel just as the engine emitted finished()
    // (race window), honour the user's intent and discard the result.
    if (mCompareCancelledByUser)
        return;

    if (!left || !right)
        return;

    QJsonModel *leftModel  = left_cont->getJsonModel();
    QJsonModel *rightModel = right_cont->getJsonModel();
    JsonDiffEngine::apply(*left,  leftModel,  rightModel);
    JsonDiffEngine::apply(*right, rightModel, leftModel);

    left_cont->gotoIndexHandler(true);
    right_cont->gotoIndexHandler(true);
}

void QJsonDiff::onCompareCancelled()
{
    if (mProgressDialog)
        mProgressDialog->hide();
    // Discard partial — no apply (Phase 2 design Q3).
}

void QJsonDiff::onCompareProgressed(int done, int total)
{
    if (!mProgressDialog) return;
    mProgressDialog->setRange(0, total);
    mProgressDialog->setValue(done);
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

    JsonDiffEngine::compare(leftSnap, rightSnap, mode);

    JsonDiffEngine::apply(leftSnap,  leftModel,  rightModel);
    JsonDiffEngine::apply(rightSnap, rightModel, leftModel);
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

