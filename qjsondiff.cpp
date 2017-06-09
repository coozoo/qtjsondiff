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
    qjsoncontainer_layout->addWidget(container_left_groupbox,2,0);
    qjsoncontainer_layout->addWidget(container_right_groupbox,2,1);
    qDebug()<<"$container_groupbox->setLayout(qjsoncontainer_layout)";
    //parent->setLayout(qjsoncontainer_layout);

    common_groupbox=new QGroupBox(parent);
    common_groupbox->setLayout(qjsoncontainer_layout);
    common_layout=new QVBoxLayout();
    parent->setLayout(common_layout);

    compare_pushbutton=new QPushButton(common_groupbox);
    compare_pushbutton->setText("Compare");
    qjsoncontainer_layout->addWidget(compare_pushbutton,0,0,1,2);
    syncScroll_checkbox=new QCheckBox(common_groupbox);
    syncScroll_checkbox->setText("Sync Scrolls");
    qjsoncontainer_layout->addWidget(syncScroll_checkbox,1,0);
    common_layout->addWidget(common_groupbox);
    connect(compare_pushbutton,SIGNAL(clicked()),this,SLOT(on_compare_pushbutton_clicked()));
    connect(left_cont->getTreeView(),SIGNAL(clicked(QModelIndex)),this,SLOT(on_lefttreeview_clicked(QModelIndex)));
    connect(right_cont->getTreeView(),SIGNAL(clicked(QModelIndex)),this,SLOT(on_righttreeview_clicked(QModelIndex)));
    connect(right_cont,SIGNAL(sOpenJsonFile()),this,SLOT(reinitLeftModel()));
    connect(left_cont,SIGNAL(sOpenJsonFile()),this,SLOT(reinitRightModel()));
    connect(right_cont,SIGNAL(jsonUpdated()),this,SLOT(reinitLeftModel()));
    connect(left_cont,SIGNAL(jsonUpdated()),this,SLOT(reinitRightModel()));
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
    right_cont->getTreeView()->repaint();
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
    left_cont->getTreeView()->repaint();
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

void QJsonDiff::paintEvent(QPaintEvent *) {
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
    compareModels(left_cont->getJsonModel(),QModelIndex(),right_cont->getJsonModel());
    fixColors(left_cont->getJsonModel(),QModelIndex());
    fixColors(right_cont->getJsonModel(),QModelIndex());
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
        QColor leftColor=QColor(Qt::red);
        QColor rightColor=QColor(Qt::red);

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
                        leftColor=QColor(0, 100, 0, 150);
                        rightColor=QColor(0, 100, 0, 150);
                    }
                    else
                    {
                        leftColor=QColor(Qt::red);
                        rightColor=QColor(Qt::red);
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
                        leftColor=QColor(0, 100, 0, 150);
                        rightColor=QColor(0, 100, 0, 150);
                    }
                    else
                    {
                        leftColor=QColor(Qt::red);
                        rightColor=QColor(Qt::red);
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
                        leftColor=QColor(0, 100, 0, 150);
                        rightColor=QColor(0, 100, 0, 150);
                    }
                    else
                    {
                        leftColor=QColor(Qt::red);
                        rightColor=QColor(Qt::red);
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
                        leftColor=QColor(0, 100, 0, 150);
                        rightColor=QColor(0, 100, 0, 150);
                    }
                    else
                    {
                        leftColor=QColor(Qt::red);
                        rightColor=QColor(Qt::red);
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
                        leftColor=QColor(0, 100, 0, 150);
                        rightColor=QColor(0, 100, 0, 150);
                    }
                    else
                    {
                        leftColor=QColor(Qt::red);
                        rightColor=QColor(Qt::red);
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
                   leftColor=QColor(0, 100, 0, 150);
                   rightColor=QColor(0, 100, 0, 150);
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
                    leftColor=QColor(Qt::red);
                    rightColor=QColor(Qt::red);
                    item->setColor(rightColor);
                    itemLeft->setColor(leftColor);
                    item->setIdxRelation(idxLeft);
                    itemLeft->setIdxRelation(idxRight);
                    modelRight->layoutChanged();
                    return 0;
        }
        }

        //item->setColor(QColor(Qt::red));
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

        if(item->color()!=QColor(0, 100, 0, 150) && item->color().isValid() && item->color()!=QColor(Qt::lightGray) && item->parent()->color()!=QColor(Qt::red) && item->parent()->color()!=QColor(Qt::lightGray))
        {
            item->parent()->setColor(QColor(Qt::yellow));
        }

        if(!item->color().isValid())
        {
           item->setColor(QColor(Qt::lightGray));

        }
        model->layoutChanged();
    }

    return 0;
}
