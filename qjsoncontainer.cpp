/* Author: Yuriy Kuzin
 * Description: widget to load JSON from file or http source
 *              or you can paste it manualy into text area
 *              evrytime model will be updated
 *              it's possible to perform search in text view and json view
 */
#include "qjsoncontainer.h"

QJsonContainer::QJsonContainer(QWidget *parent):
    QWidget(parent)
{

    copyRow = myMenu.addAction(tr("Copy Row"));
    copyRows = myMenu.addAction(tr("Copy Rows"));
    copyPath = myMenu.addAction(tr("Copy Path"));
    copyJsonPlainText=myMenu.addAction(tr("Copy Plain Json"));
    copyJsonPrettyText=myMenu.addAction(tr("Copy Pretty Json"));
    copyJsonByPath=myMenu.addAction(tr("Copy Selected Json Value"));

    qDebug() << "obj_layout parent";
    obj_layout = new QVBoxLayout(parent);
    obj_layout->setContentsMargins(QMargins(5, 5, 5, 5));


    qDebug() << "treeview_groupbox parent";
    treeview_groupbox = new QGroupBox(parent);
    treeview_groupbox->setStyleSheet("QGroupBox {  border:0;}");

    qDebug() << "treeview_layout treeview_groupbox";
    treeview_layout = new QVBoxLayout(treeview_groupbox);

    qDebug() << "treeview_layout treeview_groupbox";
    treeview = new QTreeView(treeview_groupbox);
    viewjson_plaintext = new QPlainTextEdit(treeview_groupbox);
    viewjson_plaintext->setVisible(false);

    QPalette p = viewjson_plaintext->palette();
    p.setColor(QPalette::Highlight, QColor(Qt::blue));
    p.setColor(QPalette::HighlightedText, QColor(Qt::lightGray));
    viewjson_plaintext->setPalette(p);



    showjson_pushbutton = new QPushButton(treeview_groupbox);
    showjson_pushbutton->setText(tr("Show Json Text"));
    showjson_pushbutton->setCheckable(true);

    //QJsonDocument data1=QJsonDocument::fromJson("{\"subscription\":[{\"action\":0,\"event\":{\"GroupId\":0,\"Id\":0,\"IdType\":1},\"league\":{\"GroupId\":10001,\"Id\":1,\"IdType\":0},\"scores\":[1,3,2],\"sportId\":4},{\"action\":0,\"event\":{\"GroupId\":0,\"Id\":0,\"IdType\":1},\"league\":{\"GroupId\":1000,\"Id\":1,\"IdType\":0},\"scores\":[1,2],\"sportId\":4}]}");
    //QString datastr=data1.toJson();
    QString datastr = "";
    model = new QJsonModel(parent);
    model->loadJson(datastr.toUtf8());
    //connect some branch expanding signal/slot

    treeview->setModel(model);

    // add layout, insert checkbox and treview into it
    //QVBoxLayout *treeview_layout = new QVBoxLayout(parent);


    toolbar = new QToolBar();
    //toolbar->setGeometry(0,21,509,458);
    toolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    toolbar->setMovable(true);
    toolbar->setFloatable(true);
    toolbar->setAllowedAreas(Qt::AllToolBarAreas);

    qDebug() << "expandAll_Checkbox treeview_groupbox";
    expandAll_Checkbox = new QCheckBox(toolbar);
    expandAll_Checkbox->setText(QString(tr("Expand")));

    sortObj_toolButton = new QToolButton(toolbar);
    sortObj_toolButton->setText(tr("Sort"));
    sortObj_toolButton->setToolTip(tr("Sort objects inside array\nmaybe helpful when order does not relevant"));
    sortObj_toolButton->setHidden(true);

    tools_layout = new QGridLayout(toolbar);
    tools_layout->setContentsMargins(0, 0, 0, 0);
    find_lineEdit = new QLineEdit();
    find_lineEdit->setPlaceholderText(tr("Serach for..."));
    find_lineEdit->setToolTip(tr("Enter text and press enter to search"));
    find_lineEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    findNext_toolbutton = new QToolButton(toolbar);
    findNext_toolbutton->setText(">>");
    findNext_toolbutton->setToolTip("Find Next");
    findPrevious_toolbutton = new QToolButton(toolbar);
    findPrevious_toolbutton->setText("<<");
    findPrevious_toolbutton->setToolTip("Find Previous");
    findCaseSensitivity_toolbutton = new QToolButton(toolbar);
    findCaseSensitivity_toolbutton->setCheckable(true);
    findCaseSensitivity_toolbutton->setIcon(QIcon(QPixmap(":/images/casesensitivity.png")));
    findCaseSensitivity_toolbutton->setToolTip("Check to make case sensetive");
    //findCaseSensitivity_toolbutton->setChecked(true);
    //tools_layout->addWidget(expandAll_Checkbox,0,Qt::AlignLeft);
    toolbar->addWidget(expandAll_Checkbox);
    toolbar->addSeparator();
    toolbar->addWidget(find_lineEdit);
    toolbar->addWidget(findPrevious_toolbutton);
    toolbar->addWidget(findNext_toolbutton);
    toolbar->addWidget(findCaseSensitivity_toolbutton);
    toolbar->addSeparator();

    spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    toolbar->addWidget(sortObj_toolButton);

    tools_layout->addWidget(toolbar, 0, 0);


    currentFindText = find_lineEdit->text();
    currentFindIndexId = -1;

    browse_groupBox = new QGroupBox(treeview_groupbox);
    browse_layout = new QHBoxLayout(browse_groupBox);
    browse_layout->setContentsMargins(0, 0, 0, 0);
    filePath_lineEdit = new QLineEdit(browse_groupBox);
    filePath_lineEdit->setPlaceholderText(tr("Select file or use URL to load json data"));
    filePath_lineEdit->setToolTip(tr("Select file or use full path/URL and hit enter to load json data"));
    browse_toolButton = new QToolButton(browse_groupBox);
    browse_toolButton->setText(tr("..."));
    //filePath_lineEdit->setStyleSheet("border: 2");
    qDebug() << "treeview_layout adding widgets";
    browse_layout->addWidget(filePath_lineEdit, 1);
    browse_layout->addWidget(browse_toolButton, 0);
    browse_groupBox->setLayout(browse_layout);
    treeview_layout->addWidget(browse_groupBox, 0);

    //treeview_layout->addWidget(expandAll_Checkbox,0,Qt::AlignLeft);
    treeview_layout->addLayout(tools_layout);
    treeview_layout->addWidget(treeview);
    treeview_layout->addWidget(viewjson_plaintext);
    treeview_layout->addWidget(showjson_pushbutton);


    qDebug() << "treeview size";
    treeview->resizeColumnToContents(0);
    treeview->resizeColumnToContents(1);
    treeview->resizeColumnToContents(2);
    //treeview->setColumnWidth(2,300);
    treeview->autoScrollMargin();
    treeview->setAlternatingRowColors(true);
    treeview->setAnimated(true);
    treeview->setRootIsDecorated(true);


    //treeview->setMinimumSize(500,500);

    //read treeview stylesheet file from app resources
    QFile file(":/qss/treeview.qss");
    if (file.open(QIODevice::ReadOnly))
        {
            QString styleSheet = file.readAll();
            file.close();
            //apply stylesheet to treeview
            treeview->setStyleSheet(styleSheet);
        }
    treeview->ensurePolished();
    treeview->setContextMenuPolicy(Qt::CustomContextMenu);
    treeview->installEventFilter(this);


    qDebug() << "obj_layout add widget treeview_groupbox";
    obj_layout->addWidget(treeview_groupbox);
    qDebug() << "parent->setLayout(obj_layout)";
    parent->setLayout(obj_layout);
    //connect right click(context menu) signal/slot
    connect(treeview, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(treeview, SIGNAL(expanded(const QModelIndex &)), this, SLOT(on_treeview_item_expanded()));
    connect(expandAll_Checkbox, SIGNAL(stateChanged(int)), this, SLOT(on_expandAll_checkbox_marked()));
    connect(browse_toolButton, SIGNAL(clicked()), this, SLOT(on_browse_toolButton_clicked()));
    connect(sortObj_toolButton, SIGNAL(clicked()), this, SLOT(on_sortObj_toolButton_clicked()));
    connect(filePath_lineEdit, SIGNAL(returnPressed()), this, SLOT(openJsonFile()));
    connect(showjson_pushbutton, SIGNAL(clicked()), this, SLOT(on_showjson_pushbutton_clicked()));
    connect(find_lineEdit, SIGNAL(returnPressed()), this, SLOT(findText()));
    connect(find_lineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_find_lineEdit_textChanged(QString)));
    connect(findNext_toolbutton, SIGNAL(clicked()), this, SLOT(on_findNext_toolbutton_clicked()));
    connect(findPrevious_toolbutton, SIGNAL(clicked()), this, SLOT(on_findPrevious_toolbutton_clicked()));
    connect(findCaseSensitivity_toolbutton, SIGNAL(clicked()), this, SLOT(on_findCaseSensitivity_toolbutton_clicked()));
    connect(model, SIGNAL(dataUpdated()), this, SLOT(on_model_dataUpdated()));

}

QJsonContainer::~QJsonContainer()
{
    model->deleteLater();
    treeview->deleteLater();
    expandAll_Checkbox->deleteLater();
    browse_groupBox->deleteLater();
    filePath_lineEdit->deleteLater();
    browse_toolButton->deleteLater();
    browse_layout->deleteLater();
    treeview_layout->deleteLater();
    treeview_groupbox->deleteLater();
}

void QJsonContainer::showContextMenu(const QPoint &point)
{


    //QModelIndex index = treeview->indexAt(point);
    //if (index.isValid() && index.row() % 2 == 0) {
    //    myMenu.exec(treeview->viewport()->mapToGlobal(point));
    //}

    //QTreeView *treeview = (QTreeView *)sender();
    QTreeView *treeview = static_cast<QTreeView *>(sender());
    // for most widgets

    QPoint globalPos = treeview->mapToGlobal(point);
    // for QAbstractScrollArea and derived classes you would use:
    // QPoint globalPos = myWidget->viewport()->mapToGlobal(pos);

    QTextStream cout(stdout);
    QClipboard *clip = QApplication::clipboard();
    QAction *selectedItem = myMenu.exec(globalPos);
    if (selectedItem == copyRow)
        {
            //int columnid = treeview->selectionModel()->currentIndex().column();
            //int rowid = treeview->selectionModel()->currentIndex().row();
            //QModelIndex idx=treeview->currentIndex();
            QModelIndex idx = treeview->indexAt(point);
            QStringList strings = extractItemTextFromModel(idx);

            //QStringList strings = extractStringsFromModel(model, QModelIndex());

            //cout<<treeview->model()->index(rowid , columnid).data().toString()<<endl;
            cout << "copyRow" << endl;
            cout << strings.join("\n") << endl;
            clip->setText(strings.join("\n"));
        }
    else if (selectedItem == copyRows)
        {
            cout << "copyRows" << endl;
            QString string = extractStringsFromModel(model, QModelIndex()).join("\n");
            cout << string << endl;
            clip->setText(string);
        }
    else if (selectedItem==copyPath)
        {
            QModelIndex idx = treeview->indexAt(point);
            QString string =model->jsonPath(idx);
            cout<<string<<endl;
            clip->setText(string);
        }
    else if(selectedItem==copyJsonPlainText)
        {
            cout<<"copyJsonPlainText"<<endl;
            clip->setText(QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8()).toJson(QJsonDocument::Compact));
        }
    else if(selectedItem==copyJsonPrettyText)
        {
            cout<<"copyJsonPrettyText"<<endl;
            clip->setText(QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8()).toJson(QJsonDocument::Indented));
        }
    else if(selectedItem==copyJsonByPath)
        {
            QModelIndex idx = treeview->indexAt(point);
            cout<<"copyJsonByPath"<<endl;
            QString string=getJson(model->jsonIndexPath(idx));
            clip->setText(string);
        }
}


void QJsonContainer::findText()
{
    //viewjson_plaintext->moveCursor(QTextCursor::Start);
    if (viewjson_plaintext->isVisible())
        {
            //ignore search case if button not pressed
            if (!findCaseSensitivity_toolbutton->isChecked())
                {
                    viewjson_plaintext->find(find_lineEdit->text());
                }
            else
                {
                    viewjson_plaintext->find(find_lineEdit->text(), QTextDocument::FindCaseSensitively);
                }
        }
    else
        {
            //works all values to stringlist
            //QStringList strings = extractStringsFromModel(model, QModelIndex());
            //QModelIndex idx=treeview->currentIndex();
            //QStringList strings = extractStringsFromModel(model, idx);

            //qDebug()<<"##################"<<strings;
            findTextJsonIndexHandler(true);

        }
}

void QJsonContainer::on_findNext_toolbutton_clicked()
{
    findText();
}

void QJsonContainer::on_findPrevious_toolbutton_clicked()
{
    if (viewjson_plaintext->isVisible())
        {

            if (!findCaseSensitivity_toolbutton->isChecked())
                {
                    viewjson_plaintext->find(find_lineEdit->text(), QTextDocument::FindBackward);
                }
            else
                {
                    viewjson_plaintext->find(find_lineEdit->text(), QTextDocument::FindBackward | QTextDocument::FindCaseSensitively);
                }
        }
    else
        {
            findTextJsonIndexHandler(false);
        }
}

void QJsonContainer::on_findCaseSensitivity_toolbutton_clicked()
{
    currentFindIndexesList.clear();
    currentFindText = find_lineEdit->text();
    currentFindIndexesList = findModelText(model, QModelIndex());
    currentFindIndexId = -1;
}

/* Move trough json items that contain text
 * Very stupid logic but it works and implementation even worse
 * input bool value:
 *      true - forward;
 *      false - backward;
 *
 */
void QJsonContainer::findTextJsonIndexHandler(bool direction)
{
    QTextStream cout(stdout);
    QModelIndex idx = treeview->currentIndex();
    if (idx.isValid() && idx.column() != 0)
        {
            //cout << idx.row() << endl;
            //idx = model->index(idx.row(), 0);
            idx=idx.siblingAtColumn(0);
        }

    if (currentFindText.isEmpty() || currentFindText != find_lineEdit->text())
        {
            currentFindIndexesList.clear();
            currentFindText = find_lineEdit->text();
            currentFindIndexesList = findModelText(model, QModelIndex());
            currentFindIndexId = -1;
            qDebug() << "##################" << currentFindIndexesList;
        }
    //cout<<"before"<<currentFindIndexId<<endl;
    if(!currentFindIndexesList.contains(idx))
        {
            int indexid = -1;
            bool matchselectedbool=false;
            currentIndexFinder(model, QModelIndex(), &currentFindIndexesList,idx,matchselectedbool,indexid);
            cout << "Found index at:" << indexid << endl;
            if(currentFindIndexId!=-1 && indexid!=currentFindIndexesList.count()-1 && indexid!=0)
                {
                    currentFindIndexId=indexid;
                }
            else if(((indexid==currentFindIndexesList.count()-1) || (indexid==0)) && !direction)
                {
                    currentFindIndexId=indexid+1;
                }
        }
    else
        {
            if(currentFindIndexId!=-1 && currentFindIndexId!=currentFindIndexesList.count()-1)
                {
                    currentFindIndexId = currentFindIndexesList.indexOf(idx);
                }
        }
    //cout<<"after"<<currentFindIndexId<<endl;
//QList<QModelIndex> tempList=currentFindIndexesList;
//        if(!direction)
//        {
//            for(int k = 0; k < (tempList.size()/2); k++) tempList.swap(k,tempList.size()-(1+k));
//        }
    if (currentFindIndexesList.count() > 0 && currentFindIndexId < currentFindIndexesList.count() - 1 && direction)
        {
            currentFindIndexId++;
        }
    else if (currentFindIndexesList.count() > 0 && currentFindIndexId > 0 && currentFindIndexId <= currentFindIndexesList.count() - 1 && !direction)
        {
            currentFindIndexId--;
        }

    else if (currentFindIndexesList.count() > 0 && (currentFindIndexId == currentFindIndexesList.count() - 1 || currentFindIndexId == 0))
        {
            QRect widgetRect = treeview->visualRect(currentFindIndexesList[currentFindIndexId]);
            QToolTip::showText(treeview->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), (!direction) ? tr("Start of json has been reached! Next item will be from the end") : tr("End of json has been reached! Next item will be from the start"), treeview, QRect(100, 200, 11, 16), 3000);
            currentFindIndexId = -1;
        }
    else if (((currentFindIndexesList.count() > 0 && currentFindIndexId == -1) || (currentFindIndexId>currentFindIndexesList.count()-1)) && !direction)
        {
            currentFindIndexId = currentFindIndexesList.count() - 1;
        }
    if (currentFindIndexesList.count() == 0)
        {
            QRect widgetRect = find_lineEdit->contentsRect();
            QToolTip::showText(find_lineEdit->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), tr("<b><font \"color\"=red>Text Not Found!</font></b>"), nullptr, QRect(100, 200, 11, 16), 3000);
        }
    if (currentFindIndexId >= 0)
        {
            treeview->setCurrentIndex(currentFindIndexesList[currentFindIndexId]);
        }
}

void QJsonContainer::setBrowseVisible(bool state)
{
    browse_groupBox->setVisible(state);
}

void QJsonContainer::on_showjson_pushbutton_clicked()
{
    if (viewjson_plaintext->isVisible())
        {
            viewjson_plaintext->setVisible(false);
            treeview->setVisible(true);
            loadJson(viewjson_plaintext->toPlainText());
            showjson_pushbutton->setText(tr("Show Json Text"));
        }
    else
        {
            viewjson_plaintext->setVisible(true);
            treeview->setVisible(false);
            showjson_pushbutton->setText(tr("Show Json View"));
        }
    emit jsonUpdated();
}

void QJsonContainer::on_browse_toolButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Files (*.json *.jsn *.txt);;All Files (*)"));
    filePath_lineEdit->setText(fileName);
    openJsonFile();

}

void QJsonContainer::openJsonFile()
{
    if (!filePath_lineEdit->text().contains("http://", Qt::CaseInsensitive) && !filePath_lineEdit->text().contains("https://", Qt::CaseInsensitive))
        {
            qDebug() << "reloading file";
            QFile file(filePath_lineEdit->text());
            if (file.open(QIODevice::ReadOnly))
                {
                    QString data = file.readAll();
                    file.close();
                    loadJson(data);
                }
        }
    else
        {
            qDebug() << "downloading";
            getData();
        }
    emit sOpenJsonFile();
}

void QJsonContainer::loadJson(QJsonDocument data)
{
    QString datastr = data.toJson();
    viewjson_plaintext->setPlainText((QJsonDocument::fromJson(datastr.toUtf8())).toJson(QJsonDocument::Indented));
    model->loadJson(datastr.toUtf8());
    on_expandAll_checkbox_marked();
    //treeview->setColumnWidth(2,300);
}

void QJsonContainer::loadJson(QString data)
{
    viewjson_plaintext->setPlainText((QJsonDocument::fromJson(data.toUtf8())).toJson(QJsonDocument::Indented));
    model->loadJson(data.toUtf8());
    on_expandAll_checkbox_marked();
    //treeview->setColumnWidth(2,300);
}

QString QJsonContainer::getJson(QList<QModelIndex> jsonPath)
{
    QTextStream cout(stdout);
    QString json="{}";
    QJsonDocument jsonDoc=QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8());
    QJsonValue tempValue=QJsonValue();
    QJsonValue tempValuePrev=QJsonValue();
    if(jsonDoc.isObject())
        {
            tempValue=jsonDoc.object();
        }
    else if(jsonDoc.isArray())
        {
            tempValue=jsonDoc.array();
        }
    else
        {
            return json;
        }
    for (int i = 0; i < jsonPath.size(); ++i)
        {

            QJsonTreeItem *treeItem=static_cast<QJsonTreeItem *>(jsonPath[i].internalPointer());
            //cout<<tempValue[treeItem->key()].toString()<<endl;
            if(!treeItem->parent())
                {
                    cout<<i<<"root"<<endl;
                }
            else
                {
                    if(tempValue.isObject())
                        {
                            QString key=treeItem->key();
                            if(tempValue[key].isArray())
                                {
                                    json=QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue=tempValue[key].toArray();
                                }
                            else if(tempValue[key].isObject())
                                {
                                    json=QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue=tempValue[key].toObject();
                                }
                            else if(tempValue[key].isDouble())
                                {
                                    json=QString::number(tempValue[key].toDouble());
                                }
                            else if(tempValue[key].isString())
                                {
                                    json=tempValue[key].toString();
                                }
                            else if(tempValue[key].isNull())
                                {
                                    json="null";
                                }
                            else if(tempValue[key].isBool())
                                {
                                    json=QString((tempValue[key].toBool())?QString("true"):QString("false"));
                                }
                        }
                    else if(tempValue.isArray())
                        {
                            int key=treeItem->key().toInt();
                            if(tempValue[key].isArray())
                                {
                                    json=QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue=tempValue[key].toArray();
                                }
                            else if(tempValue[key].isObject())
                                {
                                    json=QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue=tempValue[key].toObject();
                                }
                            else if(tempValue[key].isDouble())
                                {
                                    json=QString::number(tempValue[key].toDouble());
                                }
                            else if(tempValue[key].isString())
                                {
                                    json=tempValue[key].toString();
                                }
                            else if(tempValue[key].isNull())
                                {
                                    json="null";
                                }
                            else if(tempValue[key].isBool())
                                {
                                    json=QString((tempValue[key].toBool())?QString("true"):QString("false"));
                                }
                        }
                }
        }

    return json;
}

void QJsonContainer::on_expandAll_checkbox_marked()
{
//qDebug()<<"marked";
    if (expandAll_Checkbox->isChecked())
        {
            treeview->expandAll();
            treeview->resizeColumnToContents(0);
            treeview->resizeColumnToContents(1);
            treeview->resizeColumnToContents(2);
            //treeview->setColumnWidth(2,300);
            //treeview->viewport()->setBackgroundRole(QPalette::Dark);
            //qDebug()<<treeview->indexAt(QPoint(50,50)).;
        }
    else
        {
            treeview->collapseAll();
        }
}

void QJsonContainer::expandIt()
{
    expandAll_Checkbox->setChecked(true);
    on_expandAll_checkbox_marked();
}

void QJsonContainer::on_treeview_item_expanded()
{
    //qDebug()<<"expanded";
    if (!expandAll_Checkbox->isChecked())
        {
            QTreeView *treeviewcall = static_cast<QTreeView *>(sender());
            treeviewcall->resizeColumnToContents(0);
            treeviewcall->resizeColumnToContents(1);
            treeviewcall->resizeColumnToContents(2);
            //treeviewcall->setColumnWidth(2,300);
        }
}

QJsonModel *QJsonContainer::getJsonModel()
{
    return model;
}

QTreeView *QJsonContainer::getTreeView()
{
    return treeview;
}

void QJsonContainer::reInitModel()
{
    loadJson(viewjson_plaintext->toPlainText().toUtf8());
}

//sort arrays inside of objects
/*QJsonDocument QJsonContainer::sortObjectArrays(QJsonDocument data)
{
    QJsonDocument resultData=data;
    if(data.isObject())
    {
        QJsonObject jsonObj=data.object();
        resultData=QJsonDocument::fromVariant(sortObjectArraysGrabObject(jsonObj).toVariantMap());
    }
    else
    {
        QJsonArray jsonArr=data.array();
        resultData=QJsonDocument::fromVariant(sortObjectArraysGrabArray(jsonArr).toVariantList());
    }
    return resultData;
}*/
//Count "weight of string" or it's rather some kind of hashcode
//from my point of view it's almost unique and possible to recognize identical string
//"weight of string"="sum charachters code numbers" * "string length"
int QJsonContainer::countStringWeight(QString inStr)
{
    int total = 0;
    for (int i = 0; i <= inStr.length() - 1; i++)
        {
            QChar ch = inStr.at(i);

            total = total + ch.unicode();
        }
    total = total * inStr.length();
    //qDebug()<<total;
    return total;
}
//condition how to sort objects in array
bool QJsonContainer::wayToSort(const QJsonValue &v1, const QJsonValue &v2)
{
    // qDebug()<<"call";
    if (v1.isObject() && v2.isObject())
        {
            return countStringWeight(QJsonDocument(v1.toObject()).toJson()) < countStringWeight(QJsonDocument(v2.toObject()).toJson());
        }
    else if (v1.isDouble() && v2.isDouble())
        {
            //qDebug()<<v1<<" "<<v2;
            return v1.toDouble() < v2.toDouble();
        }
    return -1;
}
//sort array
//need to apply patch to qt https://codereview.qt-project.org/#/c/108352/
/*QJsonArray QJsonContainer::sortObjectArraysGrabArray(QJsonArray data)
{
    int arrayId=0;
    QJsonArray resultData=data;
    if(data.at(0).isObject())
        {
            //qDebug()<<data;
            //qDebug()<<data.count();
            std::sort(data.begin(), data.end(),wayToSort);

            //qDebug()<<data;

        }
    else if(data.at(0).isDouble())
        {
            //qDebug()<<data;
            //qDebug()<<data.count();
            std::sort(data.begin(), data.end(),wayToSort);
            //qDebug()<<data;

        }
    resultData=data;
    arrayId=0;
    foreach (const QJsonValue & value, resultData)
        {
            if (value.isObject())
                {
                    //qDebug() << "Object" << value.toString();
                    resultData.removeAt(arrayId);
                    resultData.insert(arrayId,sortObjectArraysGrabObject(value.toObject()));
                }

            arrayId++;
        }
    //qDebug()<<resultData;
    return resultData;
}

QJsonObject QJsonContainer::sortObjectArraysGrabObject(QJsonObject data)
{
    QJsonObject resultData=data;
    for(QJsonObject::const_iterator iter = data.begin(); iter != data.end(); ++iter)
        {
            if (iter.value().isArray())
                {
                    //qDebug() << iter.key()  << "Array" << iter.value().toString();
                    resultData.remove(iter.key());
                    resultData.insert(iter.key(),sortObjectArraysGrabArray(iter.value().toArray()));
                }
            else if (iter.value().isObject())
                {
                    //qDebug() << iter.key() << "Object" << iter.value().toString();
                    resultData.remove(iter.key());
                    resultData.insert(iter.key(),sortObjectArraysGrabObject(iter.value().toObject()));

                }
        }
    return resultData;
}


void QJsonContainer::on_sortObj_toolButton_clicked()
{
    viewjson_plaintext->setPlainText(sortObjectArrays(QJsonDocument::fromJson((viewjson_plaintext->toPlainText().toUtf8()))).toJson());
    reInitModel();
}
*/
QByteArray QJsonContainer::gUncompress(const QByteArray &data)
{
    qDebug() << "Uncompressing...";
    if (data.size() <= 4)
        {
            qWarning("gUncompress: Input data is truncated");
            return QByteArray();
        }

    QByteArray result;

    int ret;
    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    strm.avail_in = static_cast<uint>(data.size());
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char *>(data.data()));

    ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK)
        return QByteArray();

    // run inflate()
    do
        {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = reinterpret_cast<Bytef*>(const_cast<char *>(out));

            ret = inflate(&strm, Z_NO_FLUSH);
            Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

            switch (ret)
                {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     // and fall through
                    [[clang::fallthrough]];
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&strm);
                    return QByteArray();
                }

            result.append(out, CHUNK_SIZE - static_cast<int>(strm.avail_out));
        }
    while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);
    return result;
}

void QJsonContainer::getData()
{
    qDebug() << "Starting get";
    QUrl serviceUrl = QUrl(filePath_lineEdit->text());
    loadJson("{\"status\":\"Starting Get\",\"host\":\"" + serviceUrl.url() + "\"}");

    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(serviceGetDataRequestFinished(QNetworkReply *)));
    QNetworkRequest request(serviceUrl);
    if ((serviceUrl.toString()).contains("https", Qt::CaseInsensitive))
        {
            QSslConfiguration SslConfiguration(QSslConfiguration::defaultConfiguration());
            SslConfiguration.setProtocol(QSsl::AnyProtocol);
            request.setSslConfiguration(SslConfiguration);
        }
    request.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("Accept-Encoding", "gzip, deflate");
    loadJson("{\"status\":\"Sending Get\",\"host\":\"" + serviceUrl.url() + "\"}");
    QNetworkReply *reply = networkManager->get(request);
    Q_UNUSED(reply);
}

void QJsonContainer::serviceGetDataRequestFinished(QNetworkReply *reply)
{
    qDebug() << "Reply Received";
    QString url = reply->url().toString();
    loadJson("{\"status\":\"Reply received\",\"host\":\"" + url + "\"}");
    QObject *networkManager = sender();
    networkManager->deleteLater();
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray bytes = reply->readAll();
            if (reply->rawHeader("Content-Encoding") == QString("gzip"))
                {
                    bytes = gUncompress(bytes);
                }

            qDebug() << bytes;
            loadJson(QString(bytes));
        }
    else
        {
            qDebug() << "Error";
            qDebug() << reply->error();
            qDebug() << reply->errorString();
            loadJson("{\"status\":\"Error\",\"host\":\"" + url + "\",\"Error Description\":\"" + reply->errorString() + "\"}");
        }
}

QStringList QJsonContainer::extractStringsFromModel(QJsonModel *model, const QModelIndex &parent)
{
    QStringList retval;
    int rowCount = model->rowCount(parent);
    for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);
            QModelIndex idx1 = model->index(i, 1, parent);
            QModelIndex idx2 = model->index(i, 2, parent);
            qDebug() << idx0.data(Qt::DisplayRole).toString() << idx1.data(Qt::DisplayRole).toString() << idx2.data(Qt::DisplayRole).toString();
            qDebug() << static_cast<QJsonTreeItem *>(idx0.internalPointer())->typeName();
            QJsonTreeItem *item = static_cast<QJsonTreeItem*>(idx0.internalPointer());

            if (idx0.isValid())
                {
                    retval << item->text();
                    //retval << idx0.data(Qt::DisplayRole).toString() + QString("\t") + idx2.data(Qt::DisplayRole).toString() + QString("\n");
                    //qDebug()<<idx0.data(Qt::DisplayRole).toString();
                    retval << extractStringsFromModel(model, idx0);
                }
        }

    return retval;
}

QStringList QJsonContainer::extractItemTextFromModel(const QModelIndex &parent)
{
    QStringList retval;
    QJsonTreeItem *item = static_cast<QJsonTreeItem *>(parent.internalPointer());
    if (parent.isValid())
        {
            retval << item->text();
        }

    return retval;
}

QList<QModelIndex> QJsonContainer::findModelText(QJsonModel *model, const QModelIndex &parent)
{
    QList<QModelIndex> retindex;

    QString stringToSearch = find_lineEdit->text();
    if (!findCaseSensitivity_toolbutton->isChecked())
        {
            stringToSearch = stringToSearch.toLower();
        }


    int rowCount = model->rowCount(parent);

    for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);
            QModelIndex idx1 = model->index(i, 1, parent);
            QModelIndex idx2 = model->index(i, 2, parent);
            qDebug() << idx0.data(Qt::DisplayRole).toString() << idx1.data(Qt::DisplayRole).toString() << idx2.data(Qt::DisplayRole).toString();
            qDebug() << static_cast<QJsonTreeItem *>(idx0.internalPointer())->typeName();
            //QJsonTreeItem *item = static_cast<QJsonTreeItem*>(idx0.internalPointer());

            if (idx0.isValid())
                {
                    //retval << idx0.data(Qt::DisplayRole).toString() +QString("|")+idx2.data(Qt::DisplayRole).toString();
                    qDebug() << idx0.data(Qt::DisplayRole).toString();
                    //retval << extractStringsFromModel(model, idx0);
                    QString itemText = QString(idx0.data(Qt::DisplayRole).toString() + QString("|") + idx2.data(Qt::DisplayRole).toString());
                    if (!findCaseSensitivity_toolbutton->isChecked())
                        {
                            itemText = itemText.toLower();
                        }
                    if (itemText.contains(stringToSearch))
                        {
                            retindex << idx0;

                        }
                    retindex << findModelText(model, idx0);
                }
        }

    return retindex;
}

int QJsonContainer::currentIndexFinder(QJsonModel *model, const QModelIndex &parent,
                                       QList<QModelIndex> *currentFindIndexesList, QModelIndex selectedIndex, bool &matchedSelectedIndex, int &indexid)
{
    QTextStream cout(stdout);
    int rowCount = model->rowCount(parent);

    for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);
            if (idx0.isValid())
                {

                    //cout<<"row"<<i<<endl;


                    if (currentFindIndexesList->contains(idx0))
                        {
                            //cout<<"hit in find map"<<currentFindIndexesList->count()<<endl;
                            ++indexid;
                            //cout<<indexid<<endl;
                        }
                    //cout<<idx0.data(Qt::DisplayRole).toString()<<" "<<idx1.data(Qt::DisplayRole).toString()<<" "<<idx2.data(Qt::DisplayRole).toString()<<endl;
                    //cout<<selectedIndex.internalPointer()<<" "<<idx0.internalPointer()<<endl;
                    if(selectedIndex==idx0 || selectedIndex==parent)
                        {
                            matchedSelectedIndex=true;
                            break;
                        }
                    QString state=(matchedSelectedIndex)?QString("matched"):QString("notmatched");
                    //cout<<state<<endl;
                    currentIndexFinder(model, idx0, currentFindIndexesList,selectedIndex,matchedSelectedIndex,indexid);
                    if(matchedSelectedIndex)
                        {
                            break;
                        }

                }
        }
    return indexid;
}

void QJsonContainer::on_find_lineEdit_textChanged(QString text)
{
    qDebug() << "serach text has been changed: " << text;
    resetCurrentFind();

}

void QJsonContainer::resetCurrentFind()
{
    currentFindIndexesList.clear();
    currentFindIndexId = -1;
    currentFindText = "";
}

void QJsonContainer::on_model_dataUpdated()
{
    qDebug() << "model has been changed";
    resetCurrentFind();
}

bool QJsonContainer::eventFilter(QObject* obj, QEvent *event)
{
    Q_UNUSED(obj);
    QTextStream cout(stdout);
    if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyevent=static_cast<QKeyEvent *>(event);
            if(keyevent->matches(QKeySequence::Paste))
                {
                    QClipboard *clipboard = QGuiApplication::clipboard();
                    loadJson(clipboard->text());
                    emit jsonUpdated();
                }
        }
    return false;
}
