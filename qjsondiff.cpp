/* Author: Yuriy Kuzin
 * Description: Diff like widget for JSON
 *              side by side comparison of two models
 */
#include "qjsondiff.h"
#include <QGroupBox>
#include <QTime>
#include <QtConcurrent/QtConcurrent>
#include <QThread>
#include "qjsonitem.h"

QJsonDiff::QJsonDiff(QWidget *parent):
    QWidget(parent)
{

    prevLeftScroll=0;
    prevRightScroll=0;

    qjsoncontainer_layout = new QGridLayout();
    qDebug()<<"$container_groupbox=new QGroupBox(parent)";

    container_left_groupbox=new QGroupBox(parent);
    container_left_groupbox->setContentsMargins(QMargins(0,0,0,0));
    //container_left_groupbox->setStyleSheet("border:0;");

    qDebug()<<"$cont1=new QJsonContainer(container_groupbox)";
    left_cont=new QJsonContainer(container_left_groupbox);

    container_right_groupbox=new QGroupBox(parent);
    container_right_groupbox->setContentsMargins(QMargins(0,0,0,0));
    //container_right_groupbox->setStyleSheet("border:0;");

    qDebug()<<"$cont2=new QJsonContainer(container_groupbox1)";
    right_cont=new QJsonContainer(container_right_groupbox);


    qDebug()<<"$qjsoncontainer_layout->addWidget";
    qjsoncontainer_layout->addWidget(container_left_groupbox,2,0,1,2);
    qjsoncontainer_layout->addWidget(container_right_groupbox,2,2,1,2);
    qDebug()<<"$container_groupbox->setLayout(qjsoncontainer_layout)";
    //parent->setLayout(qjsoncontainer_layout);

    common_groupbox=new QGroupBox(parent);
    common_groupbox->setLayout(qjsoncontainer_layout);
    common_layout=new QVBoxLayout();
    parent->setLayout(common_layout);

    compare_pushbutton=new QPushButton(common_groupbox);
    compare_pushbutton->setText("Compare");
    qjsoncontainer_layout->addWidget(compare_pushbutton,0,0,1,4);
    syncScroll_checkbox=new QCheckBox(common_groupbox);
    syncScroll_checkbox->setText("Sync Scrolls");
    qjsoncontainer_layout->addWidget(syncScroll_checkbox,1,0);
    useFullPath_checkbox=new QCheckBox(common_groupbox);
    useFullPath_checkbox->setText("Use Full Path");
    useFullPath_checkbox->setChecked(true);
    useFullPath_checkbox->setToolTip("Otherwise try to find child+parent pair anywhere in JSON tree");
    qjsoncontainer_layout->addWidget(useFullPath_checkbox,1,1);
    checkboxSpacer=new QSpacerItem(5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
    qjsoncontainer_layout->addItem(checkboxSpacer,1,2,1,2);
    common_layout->addWidget(common_groupbox);
    connect(compare_pushbutton,SIGNAL(clicked()),this,SLOT(on_compare_pushbutton_clicked()));
    connect(left_cont->getTreeView(),SIGNAL(clicked(QModelIndex)),this,SLOT(on_lefttreeview_clicked(QModelIndex)));
    connect(right_cont->getTreeView(),SIGNAL(clicked(QModelIndex)),this,SLOT(on_righttreeview_clicked(QModelIndex)));
    connect(right_cont,SIGNAL(sOpenJsonFile()),this,SLOT(reinitLeftModel()));
    connect(left_cont,SIGNAL(sOpenJsonFile()),this,SLOT(reinitRightModel()));
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
}

QJsonDiff::~QJsonDiff()
{
    left_cont->deleteLater();
    right_cont->deleteLater();
    qjsoncontainer_layout->deleteLater();
    container_left_groupbox->deleteLater();
    container_right_groupbox->deleteLater();
    common_groupbox->deleteLater();
    compare_pushbutton->deleteLater();
    syncScroll_checkbox->deleteLater();
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

void QJsonDiff::loadJsonLeft(QString data)
{
    left_cont->loadJson(data);
}

void QJsonDiff::loadJsonRight(QJsonDocument data)
{
    right_cont->loadJson(data);
}

void QJsonDiff::loadJsonRight(QString data)
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

void QJsonDiff::on_compare_pushbutton_clicked()
{
    qDebug()<<"compare button clicked";
    //QTreeView *treeview=left_cont->getTreeView();
    //qDebug()<<treeview->currentIndex();
    //QJsonModel *model=left_cont->getJsonModel();


    left_cont->reInitModel();
    right_cont->reInitModel();
    double temp=QTime::currentTime().msecsSinceStartOfDay();
    qDebug()<<"started:"<<temp;
    startComparison();

    qDebug()<<(QTime::currentTime().msecsSinceStartOfDay()-temp)/1000;
}

void QJsonDiff::startComparison()
{
    if(!useFullPath_checkbox->isChecked())
        {
            compareModels(left_cont->getJsonModel(),QModelIndex(),right_cont->getJsonModel());
            fixColors(left_cont->getJsonModel(),QModelIndex());
            fixColors(right_cont->getJsonModel(),QModelIndex());
        }
    else
        {
            QList<QModelIndex> leftIndexList;
            QStringList leftStringList=jsonPathList(left_cont->getJsonModel(),QModelIndex(),&leftIndexList);
            QList<QModelIndex> rightIndexList;
            QStringList rightStringList=jsonPathList(right_cont->getJsonModel(),QModelIndex(),&rightIndexList);
            comparePath(left_cont->getJsonModel(),leftStringList,leftIndexList,right_cont->getJsonModel(),rightStringList,rightIndexList);
            comparePath(right_cont->getJsonModel(),rightStringList,rightIndexList,left_cont->getJsonModel(),leftStringList,leftIndexList);
            compareValue(left_cont->getJsonModel(),leftIndexList,right_cont->getJsonModel());
            compareValue(right_cont->getJsonModel(),rightIndexList,left_cont->getJsonModel());
            fixColors(left_cont->getJsonModel(),QModelIndex());
            fixColors(right_cont->getJsonModel(),QModelIndex());
        }
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



void QJsonDiff::compareModels(QJsonModel *modelLeft, const QModelIndex &parentLeft, QJsonModel *modelRight)
{
    int rowCount = modelLeft->rowCount(parentLeft);

    for(int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = modelLeft->index(i, 0, parentLeft);
            //QModelIndex idx1 = modelLeft->index(i, 1, parentLeft);
            QModelIndex idx2 = modelLeft->index(i, 2, parentLeft);
            //qDebug()<<idx0.data(Qt::DisplayRole).toString()<<idx1.data(Qt::DisplayRole).toString()<<idx2.data(Qt::DisplayRole).toString();
            //qDebug()<<static_cast<QJsonTreeItem*>(idx0.internalPointer())->typeName();
            //QJsonTreeItem *item = static_cast<QJsonTreeItem*>(idx0.internalPointer());
            QJsonTreeItem *item=modelLeft->itemFromIndex(idx0);
            /*if(item->parent()->key()!="root")
            {
             qDebug()<<"Parent:"<<item->parent()->key()<<item->parent()->typeName()<<item->parent()->value();
            }

             qDebug()<<"Item:"<<item->key()<<item->type()<<item->typeName()<<item->value();
             qDebug()<<"Childs:"<<item->childCount()<<item->color();
             qDebug()<<"test: "<<item->childCount();*/
            //item->setColor(QColor(Qt::green));
            if(!item->color().isValid())
                {
                    int res=findIndexInModel(modelLeft,item,idx2,modelRight,QModelIndex());
                    Q_UNUSED(res);

                    //QPainter::drawLine(QAbstractItemView::visualRect(idx0).center(),
                    //                   QAbstractItemView::visualRect(idx1).center());
                    modelLeft->layoutChanged();
                }

            if(idx0.isValid())
                {
                    //retval << idx0.data(Qt::DisplayRole).toString();
                    //qDebug()<<idx0.data(Qt::DisplayRole).toString();
                    compareModels(modelLeft, idx0,modelRight);
                }
        }
}


int QJsonDiff::findIndexInModel(QJsonModel *modelLeft, QJsonTreeItem *itemLeft, QModelIndex idxLeft,
                                QJsonModel *modelRight,const QModelIndex &parentRight)
{


    int rowCount = modelRight->rowCount(parentRight);
    for(int i = 0; i < rowCount; ++i)
        {
            QColor leftColor=hugeDiffColor;
            QColor rightColor=hugeDiffColor;

            QModelIndex idx0 = modelRight->index(i, 0, parentRight);
            QModelIndex idxRight = modelRight->index(i, 0, parentRight);
            QJsonTreeItem *item=modelRight->itemFromIndex(idx0);
            /*if(item->parent()->key()!="root")
            {
             qDebug()<<"Parent:"<<item->parent()->key()<<item->parent()->typeName()<<item->parent()->value();
            }
            qDebug()<<"Item:"<<item->key()<<item->type()<<item->typeName()<<item->value();
            qDebug()<<"Childs:"<<item->childCount()<<item->color();
            qDebug()<<"test: "<<item->childCount();*/

            //First comparison step find and highlight all items
            //that have matched in type, key name and parent name
            if(itemLeft->type()==item->type())
                {
                    if(!itemLeft->color().isValid() && !item->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                        {
                            if(itemLeft->type()==QJsonValue::Array && item->type()==QJsonValue::Array)
                                //       && !item->color().isValid() && !itemLeft->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                                {
                                    if(itemLeft->childCount()==item->childCount())
                                        {
                                            leftColor=identicalDiffColor;
                                            rightColor=identicalDiffColor;
                                        }
                                    else
                                        {
                                            leftColor=hugeDiffColor;
                                            rightColor=hugeDiffColor;
                                        }
                                    item->setColor(rightColor);
                                    itemLeft->setColor(leftColor);
                                    item->setIdxRelation(idxLeft);
                                    itemLeft->setIdxRelation(idxRight);
                                    modelRight->layoutChanged();
                                    return 0;
                                }
                            else if(itemLeft->type()==QJsonValue::Object && item->type()==QJsonValue::Object)
                                //        && !item->color().isValid()  && !itemLeft->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                                {
                                    if(itemLeft->childCount()==item->childCount())
                                        {
                                            leftColor=identicalDiffColor;
                                            rightColor=identicalDiffColor;
                                        }
                                    else
                                        {
                                            leftColor=hugeDiffColor;
                                            rightColor=hugeDiffColor;
                                        }
                                    item->setColor(rightColor);
                                    itemLeft->setColor(leftColor);
                                    item->setIdxRelation(idxLeft);
                                    itemLeft->setIdxRelation(idxRight);
                                    modelRight->layoutChanged();
                                    return 0;
                                }
                            else if(itemLeft->type()==QJsonValue::Double && item->type()==QJsonValue::Double)
                                //                && !item->color().isValid()  && !itemLeft->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                                {

                                    if(itemLeft->value()==item->value())
                                        {
                                            leftColor=identicalDiffColor;
                                            rightColor=identicalDiffColor;
                                        }
                                    else
                                        {
                                            leftColor=hugeDiffColor;
                                            rightColor=hugeDiffColor;
                                        }
                                    item->setColor(rightColor);
                                    itemLeft->setColor(leftColor);
                                    item->setIdxRelation(idxLeft);
                                    itemLeft->setIdxRelation(idxRight);
                                    modelRight->layoutChanged();
                                    return 0;

                                }
                            else if(itemLeft->type()==QJsonValue::String && item->type()==QJsonValue::String)
                                //                && !item->color().isValid()  && !itemLeft->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                                {
                                    if(itemLeft->value()==item->value())
                                        {
                                            leftColor=identicalDiffColor;
                                            rightColor=identicalDiffColor;
                                        }
                                    else
                                        {
                                            leftColor=hugeDiffColor;
                                            rightColor=hugeDiffColor;
                                        }
                                    item->setColor(rightColor);
                                    itemLeft->setColor(leftColor);
                                    item->setIdxRelation(idxLeft);
                                    itemLeft->setIdxRelation(idxRight);
                                    modelRight->layoutChanged();
                                    return 0;
                                }
                            else if(itemLeft->type()==QJsonValue::Bool && item->type()==QJsonValue::Bool)
                                //                && !item->color().isValid()  && !itemLeft->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                                {
                                    if(itemLeft->value()==item->value())
                                        {
                                            leftColor=identicalDiffColor;
                                            rightColor=identicalDiffColor;
                                        }
                                    else
                                        {
                                            leftColor=hugeDiffColor;
                                            rightColor=hugeDiffColor;
                                        }
                                    item->setColor(rightColor);
                                    itemLeft->setColor(leftColor);
                                    item->setIdxRelation(idxLeft);
                                    itemLeft->setIdxRelation(idxRight);
                                    modelRight->layoutChanged();
                                    return 0;
                                }

                            else if(itemLeft->type()==QJsonValue::Null && item->type()==QJsonValue::Null)
                                //                && !item->color().isValid()  && !itemLeft->color().isValid() && itemLeft->parent()->key()==item->parent()->key())
                                {
                                    if(itemLeft->key()==item->key())
                                        {
                                            leftColor=identicalDiffColor;
                                            rightColor=identicalDiffColor;
                                        }
                                    item->setColor(rightColor);
                                    itemLeft->setColor(leftColor);
                                    item->setIdxRelation(idxLeft);
                                    itemLeft->setIdxRelation(idxRight);
                                    modelRight->layoutChanged();
                                    return 0;
                                }
                        }
                }
            else
                {
                    //Second level - find any values that where not matched in previous step
                    //but ignore type of value (for example if some value equal to null)

                    if(!itemLeft->color().isValid() && !item->color().isValid() && itemLeft->parent()->key()==item->parent()->key() && itemLeft->key()==item->key())
                        {
                            leftColor=hugeDiffColor;
                            rightColor=hugeDiffColor;
                            item->setColor(rightColor);
                            itemLeft->setColor(leftColor);
                            item->setIdxRelation(idxLeft);
                            itemLeft->setIdxRelation(idxRight);
                            modelRight->layoutChanged();
                            return 0;
                        }
                }

            //item->setColor(hugeDiffColor);
            modelRight->layoutChanged();
            //result.leftRelations=item;
            //result.rightRelations=itemLeft;
            if(idx0.isValid())
                {
                    //retval << idx0.data(Qt::DisplayRole).toString();
                    //qDebug()<<idx0.data(Qt::DisplayRole).toString();
                    findIndexInModel(modelLeft,itemLeft,idxLeft, modelRight,idx0);
                }

        }
    return -1;

}


int QJsonDiff::fixColors(QJsonModel *model, const QModelIndex &parent)
{

    int rowCount = model->rowCount(parent);

    for(int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);


            if(idx0.isValid())
                {
                    //retval << idx0.data(Qt::DisplayRole).toString();
                    //qDebug()<<idx0.data(Qt::DisplayRole).toString();
                    fixColors(model, idx0);
                }
            QJsonTreeItem *item=model->itemFromIndex(idx0);

            if(item->color()!=identicalDiffColor && item->color().isValid() && item->color()!=notPresentDiffColor && item->parent()->color()!=hugeDiffColor && item->parent()->color()!=notPresentDiffColor)
                {
                    item->parent()->setColor(moderateDiffColor);
                }

            if(!item->color().isValid())
                {
                    item->setColor(notPresentDiffColor);

                }
            model->layoutChanged();
        }

    return 0;
}


QStringList QJsonDiff::jsonPathList(QJsonModel * model, const QModelIndex &parent, QList<QModelIndex> *indexList)
{
    QStringList text;
    QTextStream cout(stdout);
    int rowCount = model->rowCount(parent);
    for(int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);
            QString path=model->jsonPath(idx0);
            //cout<<path<<endl;
            text<<path;
            indexList->append(idx0);
            if(idx0.isValid())
                {
                    text<<jsonPathList(model, idx0,indexList);
                }
        }
    return text;
}

void QJsonDiff::comparePath(QJsonModel *modelLeft, QStringList leftPathList, QList<QModelIndex> leftIndexList,
                            QJsonModel *modelRight,QStringList rightPathList, QList<QModelIndex> rightIndexList)
{
    for(int i = 0; i < leftPathList.count(); ++i)
        {
            QModelIndex idxLeft =leftIndexList[i];
            QJsonTreeItem *item=modelRight->itemFromIndex(idxLeft);
            if(item->color()!=identicalDiffColor)
                {
                    if(rightPathList.contains(leftPathList[i]))
                        {
                            item->setColor(identicalDiffColor);
                            QModelIndex idxRight =rightIndexList[rightPathList.indexOf(leftPathList[i])];
                            QJsonTreeItem *itemRight=modelRight->itemFromIndex(idxRight);
                            item->setIdxRelation(idxRight);
                            itemRight->setIdxRelation(idxLeft);
                            itemRight->setColor(identicalDiffColor);
                            modelLeft->layoutChanged();
                        }
                    else
                        {
                            item->setColor(notPresentDiffColor);
                        }
                }
        }
}

void QJsonDiff::compareValue(QJsonModel *modelLeft, QList<QModelIndex> leftIndexList,
                             QJsonModel *modelRight)
{
    for(int i = 0; i < leftIndexList.count(); ++i)
        {
            QModelIndex idxLeft =leftIndexList[i];
            QJsonTreeItem *item=modelRight->itemFromIndex(idxLeft);
            if(item->color()==identicalDiffColor)
                {
                    QModelIndex idxRight =item->idxRelation();
                    QJsonTreeItem *itemRight=modelRight->itemFromIndex(idxRight);
                    if((item->type()!=QJsonValue::Array || item->type()!=QJsonValue::Object) && item->value()!=itemRight->value())
                        {
                            item->setColor(hugeDiffColor);
                            itemRight->setColor(hugeDiffColor);
                        }
                    else
                        {
                            if(item->childCount()!=itemRight->childCount())
                                {
                                    item->setColor(hugeDiffColor);
                                    itemRight->setColor(hugeDiffColor);
                                }
                        }
                    modelLeft->layoutChanged();
                }
        }
}

void QJsonDiff::on_useFullPath_checkbox_stateChanged(int)
{
    reinitLeftModel();
}

