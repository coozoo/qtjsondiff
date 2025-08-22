/* Author: Yuriy Kuzin
 * Description: widget to load JSON from file or http source
 *              or you can paste it manualy into text area
 *              evrytime model will be updated
 *              it's possible to perform search in text view and json view
 */
#include "qjsoncontainer.h"
#include "jsonsyntaxhighlighter.h"

#include <QFileIconProvider>

#include <QDragEnterEvent>
#include <QMimeData>

#include "preferences/preferences.h"

QJsonContainer::QJsonContainer(QWidget *parent):
    QWidget(parent)
{

    QString expandSelectedRecursively_string = tr("Expand Selected Tree");
    QString collapseSelectedRecursively_string = tr("Collapse Selected Tree");

    copyRow = myMenu.addAction(tr("Copy Row"));
    copyRows = myMenu.addAction(tr("Copy Rows"));
    copyPath = myMenu.addAction(tr("Copy Path"));
    copyJsonPlainText = myMenu.addAction(tr("Copy Plain Json"));
    copyJsonPrettyText = myMenu.addAction(tr("Copy Pretty Json"));
    copyJsonByPath = myMenu.addAction(tr("Copy Selected Json Value"));
    myMenu.addSeparator();
    singleExpandSelectedRecursively = myMenu.addAction(expandSelectedRecursively_string);
    singleCollapseSelectedRecursively = myMenu.addAction(collapseSelectedRecursively_string);

    expandSelectedRecursively = multiSelectMenu.addAction(expandSelectedRecursively_string);
    collapseSelectedRecursively = multiSelectMenu.addAction(collapseSelectedRecursively_string);
    expandSelected = multiSelectMenu.addAction(tr("Expand Selected"));
    collapseSelected = multiSelectMenu.addAction(tr("Collapse Selected"));


    qDebug() << "obj_layout parent";
    obj_layout = new QVBoxLayout(parent);
    obj_layout->setContentsMargins(QMargins(0, 0, 0, 0));


    qDebug() << "treeview_groupbox parent";
    treeview_groupbox = new QGroupBox(parent);
    treeview_groupbox->setStyleSheet("QGroupBox {  border:0;}");

    qDebug() << "treeview_layout treeview_groupbox";
    treeview_layout = new QVBoxLayout(treeview_groupbox);

    qDebug() << "treeview_layout treeview_groupbox";
    treeview = new QTreeView(treeview_groupbox);
    treeview->setSelectionMode(QAbstractItemView::ExtendedSelection);
    viewjson_plaintext = new QPlainTextEdit(treeview_groupbox);
    viewjson_plaintext->setVisible(false);
    viewjson_plaintext->document()->setDefaultFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    viewJsonSyntaxHighlighter = new JsonSyntaxHighlighter(viewjson_plaintext->document());

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
    expandAll_Checkbox = new QAction(toolbar);
    expandAll_Checkbox->setCheckable(true);
    expandAll_Checkbox->setIcon(QIcon(QPixmap(":/images/tree_collapsed.png")));
    expandAll_Checkbox->setText(QString(tr("Expand")));
    expandAll_Checkbox->setToolTip(QString(tr("Expand")));

    sortObj_toolButton = new QAction(toolbar);
    sortObj_toolButton->setIcon(QIcon(QPixmap(":/images/sort.png")));
    sortObj_toolButton->setText(tr("Sort"));
    sortObj_toolButton->setToolTip(tr("Sort objects inside array\nIt can be helpful for comparison when order does not relevant"));
    //sortObj_toolButton->setHidden(true);

    switchview_action = new QAction(toolbar);
    switchview_action->setIcon(QIcon(QPixmap(":/images/switch.png")));
    switchview_action->setText(tr("Switch View"));
    switchview_action->setToolTip(tr("Switch between tree/text view"));

    //showjson_pushbutton = new QPushButton(treeview_groupbox);
    showjson_pushbutton = new QPushButton(this);
    showjson_pushbutton->setText(tr("Show Json Text"));
    showjson_pushbutton->setToolTip(tr("Switch between view modes"));
    showjson_pushbutton->setCheckable(true);

    tools_layout = new QGridLayout(toolbar);
    tools_layout->setContentsMargins(0, 0, 0, 0);
    find_lineEdit = new QLineEdit();
    find_lineEdit->setPlaceholderText(tr("Serach for..."));
    find_lineEdit->setToolTip(tr("Enter text and press enter to search"));
    find_lineEdit->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    findNext_toolbutton = new QAction(toolbar);
    QIcon findNext_icon=QIcon(createPixmapFromText(tr(">>")));
    findNext_toolbutton->setText(tr("Search Next"));
    findNext_toolbutton->setIcon(findNext_icon);
    findNext_toolbutton->setToolTip(tr("Find Next"));
    findPrevious_toolbutton = new QAction(toolbar);
    QIcon findPrevious_icon=QIcon(createPixmapFromText(tr("<<")));
    findPrevious_toolbutton->setText(tr("Search Previous"));
    findPrevious_toolbutton->setIcon(findPrevious_icon);
    findPrevious_toolbutton->setToolTip(tr("Find Previous"));
    findCaseSensitivity_toolbutton = new QAction(toolbar);
    findCaseSensitivity_toolbutton->setCheckable(true);
    findCaseSensitivity_toolbutton->setText(tr("Search case sensetive"));
    findCaseSensitivity_toolbutton->setIcon(QIcon(QPixmap(":/images/casesensitivity.png")));
    findCaseSensitivity_toolbutton->setToolTip(tr("Check to make case sensitive"));


    //findCaseSensitivity_toolbutton->setChecked(true);
    //tools_layout->addWidget(expandAll_Checkbox,0,Qt::AlignLeft);
    toolbar->addAction(switchview_action);
    toolbar->addAction(expandAll_Checkbox);
    toolbar->addSeparator();
    toolbar->addWidget(find_lineEdit);
    toolbar->addAction(findPrevious_toolbutton);
    toolbar->addAction(findNext_toolbutton);
    toolbar->addAction(findCaseSensitivity_toolbutton);
    toolbar->addSeparator();
    toolbar->setIconSize(QSize(28,28));


    spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    //toolbar->addWidget(sortObj_toolButton);
    toolbar->addAction(sortObj_toolButton);
    toolbar->addWidget(spacer);

    tools_layout->addWidget(toolbar, 0, 0);
    //tools_layout->setMenuBar(toolbar);


    currentFindText = find_lineEdit->text();
    currentFindIndexId = -1;

    browse_groupBox = new QGroupBox(treeview_groupbox);
    browse_layout = new QHBoxLayout(browse_groupBox);
    browse_layout->setContentsMargins(0, 0, 0, 0);
    filePath_lineEdit = new QLineEdit(browse_groupBox);
    filePath_lineEdit->setPlaceholderText(tr("Select file or use URL to load json data"));
    filePath_lineEdit->setToolTip(tr("Select file or use full path/URL and hit enter to load json data"));
    filePath_lineEdit->setFixedHeight(28);
    browse_toolButton = new QToolButton(browse_groupBox);
    browse_toolButton->setIcon(QFileIconProvider().icon(QFileIconProvider::File));
    browse_toolButton->setToolTip(tr("Open file"));
    browse_toolButton->setFixedSize(28, 28);
    refresh_toolButton = new QToolButton(browse_groupBox);
    refresh_toolButton->setIcon(QIcon(QPixmap(":/images/refresh.png")));
    refresh_toolButton->setToolTip(tr("Reload"));
    refresh_toolButton->setFixedSize(28, 28);
    //filePath_lineEdit->setStyleSheet("border: 2");
    qDebug() << "treeview_layout adding widgets";
    browse_layout->addWidget(filePath_lineEdit, 1);
    browse_layout->addWidget(browse_toolButton, 0);
    browse_layout->addWidget(refresh_toolButton, 1);
    browse_groupBox->setLayout(browse_layout);
    treeview_layout->addWidget(browse_groupBox, 0);

    //treeview_layout->addWidget(expandAll_Checkbox,0,Qt::AlignLeft);
    treeview_layout->addLayout(tools_layout);
    treeview_layout->addWidget(treeview);
    treeview_layout->addWidget(viewjson_plaintext);
    treeview_layout->addWidget(showjson_pushbutton);

    showJsonButtonPosition();

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

    //some arangments to make drag'n'drop posible
    treeview->setAcceptDrops(false);
    viewjson_plaintext->setAcceptDrops(false);
    treeview_groupbox->setAcceptDrops(true);
    treeview->installEventFilter(this);
    treeview_groupbox->installEventFilter(this);

    qDebug() << "obj_layout add widget treeview_groupbox";
    obj_layout->addWidget(treeview_groupbox);
    qDebug() << "parent->setLayout(obj_layout)";
    parent->setLayout(obj_layout);
    //connect right click(context menu) signal/slot
    connect(treeview, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(treeview, SIGNAL(expanded(const QModelIndex &)), this, SLOT(on_treeview_item_expanded()));
    //connect(expandAll_Checkbox, SIGNAL(stateChanged(int)), this, SLOT(on_expandAll_checkbox_marked()));
    connect(expandAll_Checkbox, &QAction::triggered, this,&QJsonContainer::on_expandAll_checkbox_marked);
    connect(browse_toolButton, SIGNAL(clicked()), this, SLOT(on_browse_toolButton_clicked()));
    connect(refresh_toolButton, &QToolButton::clicked, this, &QJsonContainer::on_refresh_toolButton_clicked);
    //connect(sortObj_toolButton, SIGNAL(clicked()), this, SLOT(on_sortObj_toolButton_clicked()));
    connect(sortObj_toolButton,&QAction::triggered,this,&QJsonContainer::on_sortObj_toolButton_clicked);
    connect(switchview_action,&QAction::triggered,showjson_pushbutton,&QPushButton::click);
    connect(filePath_lineEdit, SIGNAL(returnPressed()), this, SLOT(openJsonFile()));
    connect(showjson_pushbutton, SIGNAL(clicked()), this, SLOT(on_showjson_pushbutton_clicked()));
    connect(find_lineEdit, SIGNAL(returnPressed()), this, SLOT(findText()));
    connect(find_lineEdit, SIGNAL(textChanged(QString)), this, SLOT(on_find_lineEdit_textChanged(QString)));
    //connect(findNext_toolbutton, SIGNAL(clicked()), this, SLOT(on_findNext_toolbutton_clicked()));
    //connect(findPrevious_toolbutton, SIGNAL(clicked()), this, SLOT(on_findPrevious_toolbutton_clicked()));
    connect(findNext_toolbutton, &QAction::triggered, this, &QJsonContainer::on_findNext_toolbutton_clicked);
    connect(findPrevious_toolbutton, &QAction::triggered, this, &QJsonContainer::on_findPrevious_toolbutton_clicked);
    //connect(findCaseSensitivity_toolbutton, SIGNAL(clicked()), this, SLOT(on_findCaseSensitivity_toolbutton_clicked()));
    connect(findCaseSensitivity_toolbutton, &QAction::triggered, this, &QJsonContainer::on_findCaseSensitivity_toolbutton_clicked);
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

void QJsonContainer::showGoto(bool show)
{
    //no way to hide :)
    if(show)
    {
        goToNextDiff_toolbutton=new QAction(toolbar);
        QIcon goToNextDiff_icon=QIcon(createPixmapFromText(tr("->|")));
        goToNextDiff_toolbutton->setIcon(goToNextDiff_icon);
        goToNextDiff_toolbutton->setText(tr("Go to Next Diff"));
        goToNextDiff_toolbutton->setToolTip(tr("Go to Next Diff"));
        goToPreviousDiff_toolbutton=new QAction(toolbar);
        QIcon goToPreviousDiff_icon=QIcon(createPixmapFromText(tr("|<-")));
        goToPreviousDiff_toolbutton->setIcon(goToPreviousDiff_icon);
        goToPreviousDiff_toolbutton->setText(tr("Go to Previous Diff"));
        goToPreviousDiff_toolbutton->setToolTip(tr("Go to Previous Diff"));
        diffAmount_lineEdit=new QLineEdit(toolbar);
        diffAmount_lineEdit->setText("0");
        diffAmount_lineEdit->setToolTip(tr("Amount of Differences (without root objects/arrays, means values only)"));
        diffAmount_lineEdit->setReadOnly(true);
        diffAmount_lineEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        diffAmount_lineEdit->setFixedWidth(QFontMetrics(diffAmount_lineEdit->font()).maxWidth());
        diffAmount_lineEdit->setAlignment(Qt::AlignCenter);
        gotDefaultPalette=diffAmount_lineEdit->palette();
        toolbar->addSeparator();
        toolbar->addWidget(diffAmount_lineEdit);
        toolbar->addAction(goToPreviousDiff_toolbutton);
        toolbar->addAction(goToNextDiff_toolbutton);
        toolbar->addSeparator();
        connect(goToPreviousDiff_toolbutton,&QAction::triggered,this,&QJsonContainer::on_GoToPreviousDiff_toolbutton_clicked);
        connect(goToNextDiff_toolbutton,&QAction::triggered,this,&QJsonContainer::on_GoToNextDiff_toolbutton_clicked);
    }
}

QPixmap QJsonContainer::createPixmapFromText(const QString &text)
{
    //looks stupid? it is stupid!
    #ifdef Q_OS_WIN
        QPixmap pix(385,385);
    #else
        QPixmap pix(256,256);
    #endif
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setPen(QPen(Qt::Dense4Pattern,Qt::black));
    painter.setFont(QFont("Times", 220, QFont::Bold));
    #ifdef Q_OS_WIN
        painter.drawText(QRect(0,0,385, 385),Qt::AlignHCenter, text);
    #else
        painter.drawText(QRect(0,0,256, 256),Qt::AlignHCenter, text);
    #endif
    painter.end();
    return pix;
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

    if (treeview->selectionModel()->selectedRows().size() == 1)
        {
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

                    //qDebug()<<treeview->model()->index(rowid , columnid).data().toString();
                    qDebug() << "copyRow";
                    qDebug() << strings.join("\n");
                    clip->setText(strings.join("\n"));
                }
            else if (selectedItem == copyRows)
                {
                    qDebug() << "copyRows";
                    QString string = extractStringsFromModel(model, QModelIndex()).join("\n");
                    qDebug() << string;
                    clip->setText(string);
                }
            else if (selectedItem == copyPath)
                {
                    QModelIndex idx = treeview->indexAt(point);
                    QString string = model->jsonPath(idx);
                    qDebug() << string;
                    clip->setText(string);
                }
            else if (selectedItem == copyJsonPlainText)
                {
                    qDebug() << "copyJsonPlainText";
                    clip->setText(QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8()).toJson(QJsonDocument::Compact));
                }
            else if (selectedItem == copyJsonPrettyText)
                {
                    qDebug() << "copyJsonPrettyText";
                    clip->setText(QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8()).toJson(QJsonDocument::Indented));
                }
            else if (selectedItem == copyJsonByPath)
                {
                    QModelIndex idx = treeview->indexAt(point);
                    qDebug() << "copyJsonByPath";
                    QString string = getJson(model->jsonIndexPath(idx));
                    clip->setText(string);
                }
            else if (selectedItem == singleExpandSelectedRecursively)
                {
                    QModelIndexList selection = treeview->selectionModel()->selectedRows();
                    for (int i = 0; i < selection.count(); i++)
                        {
                            QModelIndex index = selection.at(i);
                            expandRecursively(index, treeview, true);

                        }
                }
            else if (selectedItem == singleCollapseSelectedRecursively)
                {
                    QModelIndexList selection = treeview->selectionModel()->selectedRows();
                    for (int i = 0; i < selection.count(); i++)
                        {
                            QModelIndex index = selection.at(i);
                            expandRecursively(index, treeview, false);
                        }
                }
        }
    else
        {
            QAction *selectedItem = multiSelectMenu.exec(globalPos);
            if (selectedItem == expandSelected)
                {
                    QModelIndexList selection = treeview->selectionModel()->selectedRows();
                    for (int i = 0; i < selection.count(); i++)
                        {
                            QModelIndex index = selection.at(i);
                            treeview->setExpanded(index, true);
                        }
                }
            else if (selectedItem == collapseSelected)
                {
                    QModelIndexList selection = treeview->selectionModel()->selectedRows();
                    for (int i = 0; i < selection.count(); i++)
                        {
                            QModelIndex index = selection.at(i);
                            treeview->setExpanded(index, false);
                        }
                }
            if (selectedItem == expandSelectedRecursively)
                {
                    QModelIndexList selection = treeview->selectionModel()->selectedRows();
                    for (int i = 0; i < selection.count(); i++)
                        {
                            QModelIndex index = selection.at(i);
                            // From 5.13 qt provides native expandRecursively function but it acts weirdly
                            // when calling it in loop in some reason only first one expanded completly
                            // and I don't wanna to change qt version yet
                            expandRecursively(index, treeview, true);

                        }
                }
            if (selectedItem == collapseSelectedRecursively)
                {
                    QModelIndexList selection = treeview->selectionModel()->selectedRows();
                    for (int i = 0; i < selection.count(); i++)
                        {
                            QModelIndex index = selection.at(i);
                            expandRecursively(index, treeview, false);

                        }
                }

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
    QModelIndex idx = treeview->currentIndex();
    if (idx.isValid() && idx.column() != 0)
        {
            //qDebug() << idx.row();
            //idx = model->index(idx.row(), 0);
            idx = idx.siblingAtColumn(0);
        }

    if (currentFindText.isEmpty() || currentFindText != find_lineEdit->text())
        {
            currentFindIndexesList.clear();
            currentFindText = find_lineEdit->text();
            currentFindIndexesList = findModelText(model, QModelIndex());
            currentFindIndexId = -1;
            qDebug() << "##################" << currentFindIndexesList;
        }
    //qDebug()<<"before"<<currentFindIndexId;
    if (!currentFindIndexesList.contains(idx))
        {
            int indexid = -1;
            bool matchselectedbool = false;
            currentIndexFinder(model, QModelIndex(), &currentFindIndexesList, idx, matchselectedbool, indexid);
            qDebug() << "Found index at:" << indexid;
            if (currentFindIndexId != -1 && indexid != currentFindIndexesList.count() - 1 && indexid != 0)
                {
                    currentFindIndexId = indexid;
                }
            else if (((indexid == currentFindIndexesList.count() - 1) || (indexid == 0)) && !direction)
                {
                    currentFindIndexId = indexid + 1;
                }
        }
    else
        {
            if (currentFindIndexId != -1 && currentFindIndexId != currentFindIndexesList.count() - 1)
                {
                    currentFindIndexId = currentFindIndexesList.indexOf(idx);
                }
        }
    //qDebug()<<"after"<<currentFindIndexId;
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
    else if (((currentFindIndexesList.count() > 0 && currentFindIndexId == -1) || (currentFindIndexId > currentFindIndexesList.count() - 1)) && !direction)
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
    QString dialogTitle = tr("Open File");
    QString dialogFilter = tr("Files (*.json *.jsn *.txt);;All Files (*)");
    QString dialogDir = "";
    if (!filePath_lineEdit->text().isEmpty()) {
        dialogDir = filePath_lineEdit->text();
    }
    QString fileName = QFileDialog::getOpenFileName(this, dialogTitle, dialogDir, dialogFilter);
    if (!fileName.isEmpty()) {
        filePath_lineEdit->setText(fileName);
        openJsonFile();
    }

}

void QJsonContainer::loadJsonFile(QString target)
{
    filePath_lineEdit->setText(target);
    openJsonFile();
}

void QJsonContainer::on_refresh_toolButton_clicked()
{
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
    emit sJsonFileLoaded(filePath_lineEdit->text());
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
    QString json = "{}";
    QJsonDocument jsonDoc = QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8());
    QJsonValue tempValue = QJsonValue();
    //QJsonValue tempValuePrev = QJsonValue();
    if (jsonDoc.isObject())
        {
            tempValue = jsonDoc.object();
        }
    else if (jsonDoc.isArray())
        {
            tempValue = jsonDoc.array();
        }
    else
        {
            return json;
        }
    for (int i = 0; i < jsonPath.size(); ++i)
        {

            QJsonTreeItem *treeItem = static_cast<QJsonTreeItem *>(jsonPath[i].internalPointer());
            //qDebug()<<tempValue[treeItem->key()].toString();
            if (!treeItem->parent())
                {
                    qDebug() << i << "root";
                }
            else
                {
                    if (tempValue.isObject())
                        {
                            QString key = treeItem->key();
                            if (tempValue[key].isArray())
                                {
                                    json = QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue = tempValue[key].toArray();
                                }
                            else if (tempValue[key].isObject())
                                {
                                    json = QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue = tempValue[key].toObject();
                                }
                            else if (tempValue[key].isDouble())
                                {
                                    json = QString::number(tempValue[key].toDouble());
                                }
                            else if (tempValue[key].isString())
                                {
                                    json = tempValue[key].toString();
                                }
                            else if (tempValue[key].isNull())
                                {
                                    json = "null";
                                }
                            else if (tempValue[key].isBool())
                                {
                                    json = QString((tempValue[key].toBool()) ? QString("true") : QString("false"));
                                }
                        }
                    else if (tempValue.isArray())
                        {
                            int key = treeItem->key().toInt();
                            if (tempValue[key].isArray())
                                {
                                    json = QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue = tempValue[key].toArray();
                                }
                            else if (tempValue[key].isObject())
                                {
                                    json = QJsonDocument::fromVariant(tempValue[key].toVariant()).toJson(QJsonDocument::Indented);
                                    tempValue = tempValue[key].toObject();
                                }
                            else if (tempValue[key].isDouble())
                                {
                                    json = QString::number(tempValue[key].toDouble());
                                }
                            else if (tempValue[key].isString())
                                {
                                    json = tempValue[key].toString();
                                }
                            else if (tempValue[key].isNull())
                                {
                                    json = "null";
                                }
                            else if (tempValue[key].isBool())
                                {
                                    json = QString((tempValue[key].toBool()) ? QString("true") : QString("false"));
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
            expandAll_Checkbox->setIcon(QIcon(QPixmap(":/images/tree_collapsed.png")));
            expandAll_Checkbox->setToolTip(tr("Collapse"));
            //treeview->setColumnWidth(2,300);
            //treeview->viewport()->setBackgroundRole(QPalette::Dark);
            //qDebug()<<treeview->indexAt(QPoint(50,50)).;
        }
    else
        {
            treeview->collapseAll();
            expandAll_Checkbox->setIcon(QIcon(QPixmap(":/images/tree_expanded.png")));
            expandAll_Checkbox->setToolTip(tr("Expand"));
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
QJsonDocument QJsonContainer::sortObjectArrays(QJsonDocument data)
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
}
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
    return true;
}
//sort array
//need to apply patch to qt https://codereview.qt-project.org/#/c/108352/
QJsonArray QJsonContainer::sortObjectArraysGrabArray(QJsonArray data)
{
    int arrayId=0;
    QJsonArray resultData;
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
    emit jsonUpdated();
    viewjson_plaintext->setPlainText(sortObjectArrays(QJsonDocument::fromJson((viewjson_plaintext->toPlainText().toUtf8()))).toJson());
    reInitModel();

}

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
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));

    ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK)
        return QByteArray();

    // run inflate()
    do
        {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = reinterpret_cast<Bytef *>(const_cast<char *>(out));

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
    Q_UNUSED(reply)
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
            QJsonTreeItem *item = static_cast<QJsonTreeItem *>(idx0.internalPointer());

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
                                       QList<QModelIndex> *currentIndexesList, QModelIndex selectedIndex, bool &matchedSelectedIndex, int &indexid)
{
    int rowCount = model->rowCount(parent);

    for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);
            if (idx0.isValid())
                {

                    //qDebug()<<"row"<<i;


                    if (currentIndexesList->contains(idx0))
                        {
                            //qDebug()<<"hit in find map"<<currentFindIndexesList->count();
                            ++indexid;
                            //qDebug()<<indexid;
                        }
                    //qDebug()<<idx0.data(Qt::DisplayRole).toString()<<" "<<idx1.data(Qt::DisplayRole).toString()<<" "<<idx2.data(Qt::DisplayRole).toString();
                    //qDebug()<<selectedIndex.internalPointer()<<" "<<idx0.internalPointer();
                    if (selectedIndex == idx0 || selectedIndex == parent)
                        {
                            matchedSelectedIndex = true;
                            break;
                        }
                    QString state = (matchedSelectedIndex) ? QString("matched") : QString("notmatched");
                    //qDebug()<<state;
                    currentIndexFinder(model, idx0, currentIndexesList, selectedIndex, matchedSelectedIndex, indexid);
                    if (matchedSelectedIndex)
                        {
                            break;
                        }

                }
        }
    return indexid;
}

void QJsonContainer::on_find_lineEdit_textChanged(const QString &text)
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
    resetGoto();
    if(diffAmount_lineEdit)
    {
        diffAmountUpdate();
    }
}

bool QJsonContainer::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj)
    if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
            if (keyevent->matches(QKeySequence::Paste))
                {
                    QClipboard *clipboard = QGuiApplication::clipboard();
                    loadJson(clipboard->text());
                    emit jsonUpdated();
                }
        }
    if ( event->type() == QEvent::DragEnter)
    {
      QDragEnterEvent *dropEvent = static_cast<QDragEnterEvent*>(event);

      qDebug() << "drag enter";
      qDebug() << dropEvent->mimeData()->urls();
      if (dropEvent->mimeData()->hasUrls())
      {
          QStringList pathList;
              QList<QUrl> urlList = dropEvent->mimeData()->urls();

              for (int i = 0; i < urlList.size() && i < 32;++i)
              {
                pathList.append(urlList.at(i).toLocalFile());
              }

          if(pathList.count()>0)
          {
            dropEvent->acceptProposedAction();
            qDebug()<<pathList;
          }

      }
    }
    if ( event->type() == QEvent::Drop)
    {
      qDebug() << "drop";
      QDropEvent *dropEvent = static_cast<QDropEvent*>(event);
      qDebug() << dropEvent->mimeData()->hasUrls();
      if (dropEvent->mimeData()->hasUrls())
      {
          QStringList pathList;
              QList<QUrl> urlList = dropEvent->mimeData()->urls();

              for (int i = 0; i < urlList.size() && i < 32;++i)
              {
                pathList.append(urlList.at(i).toLocalFile());
              }

          if(pathList.count()>0)
          {
               loadJsonFile(pathList[0]);
          }

      }

    }
    qDebug()<<event->type();
    return false;
}

void QJsonContainer::expandRecursively(const QModelIndex &index, QTreeView *treeview, bool expand)
{
    if (!index.isValid())
        {
            return;
        }

    for (int i = 0; i < index.model()->rowCount(index); ++i)
        {
            const QModelIndex &childIndex = treeview->model()->index(i, 0, index);
            if (childIndex.model()->rowCount() > 0)
                {
//            qDebug()<<childIndex.model()->data(childIndex).toString();
                    expandRecursively(childIndex, treeview, expand);
                }
        }

    if (expand && !treeview->isExpanded(index))
        {
            treeview->expand(index);
        }
    else if (!expand && treeview->isExpanded(index))
        {
            treeview->collapse(index);
        }
}

void QJsonContainer::showJsonButtonPosition()
{
    //-2 bottom
    //-3 top inline
    switch (PREF_INST->showJsonButtonPosition)
    {
        case -2:
            qDebug()<<"bottom";
//            if(toolbarbutton)
//            {
//                toolbar->removeAction(toolbarbutton);
//            }
//            showjson_pushbutton->setParent(treeview_groupbox);
//            treeview_layout->addWidget(showjson_pushbutton);
            showjson_pushbutton->show();
            break;
        case -3:
            qDebug()<<"top inline";
            //treeview_layout->removeWidget(showjson_pushbutton);
//            showjson_pushbutton->setParent(toolbar);
            //toolbarbutton=toolbar->insertWidget(toolbar->actionAt(1,0),showjson_pushbutton);
//            toolbarbutton=toolbar->addWidget(showjson_pushbutton);
            showjson_pushbutton->hide();
            break;
        default:
            qDebug()<<"default";
    }
}

void QJsonContainer::on_GoToNextDiff_toolbutton_clicked()
{

    qDebug()<<gotoIndexes_list;
    gotoIndexHandler(true);

}

void QJsonContainer::on_GoToPreviousDiff_toolbutton_clicked()
{

    qDebug()<<gotoIndexes_list;
    gotoIndexHandler(false);
}

//another one crap handler :)

void QJsonContainer::gotoIndexHandler(bool directionForward)
{
    QModelIndex idx = treeview->currentIndex();
    if (idx.isValid())
        {
            if(idx.column() != 0)
                {
                    idx = idx.siblingAtColumn(0);
                }
        }
    else
        {
            idx = model->index(idx.row(), 0);
        }

    if (gotoIndexes_list.isEmpty())
        {
            gotoIndexes_list = fillGotoList(model, QModelIndex());
            diffAmountUpdate();
            currentGotoIndexId = -1;
            qDebug() << "##################indexes updates";
        }
    //qDebug()<<"before"<<currentFindIndexId;
    if (!gotoIndexes_list.contains(idx))
        {
            int indexid = -1;
            bool matchselectedbool = false;
            currentIndexFinder(model, QModelIndex(), &gotoIndexes_list, idx, matchselectedbool, indexid);
            qDebug() << "Found index at:" << indexid;
            if (currentGotoIndexId != -1 && indexid != gotoIndexes_list.count() - 1 && indexid != 0)
                {
                    currentGotoIndexId = indexid;
                }
            else if (((indexid == gotoIndexes_list.count() - 1) || (indexid == 0)) && !directionForward)
                {
                    currentGotoIndexId = indexid + 1;
                }
        }
    else
        {
            if (currentGotoIndexId != -1 && currentGotoIndexId != gotoIndexes_list.count() - 1)
                {
                    currentGotoIndexId = gotoIndexes_list.indexOf(idx);
                }
        }
    if (gotoIndexes_list.count() > 0 && currentGotoIndexId < gotoIndexes_list.count() - 1 && directionForward)
        {
             currentGotoIndexId++;
        }
    else if (gotoIndexes_list.count() > 0 && currentGotoIndexId > 0 && currentGotoIndexId <= gotoIndexes_list.count() - 1 && !directionForward)
        {
             currentGotoIndexId--;
        }

    else if (gotoIndexes_list.count() > 0 && (currentGotoIndexId == gotoIndexes_list.count() - 1 || currentGotoIndexId == 0))
        {
            QRect widgetRect = treeview->visualRect(gotoIndexes_list[currentGotoIndexId]);
            QToolTip::showText(treeview->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), (!directionForward) ? tr("Start of json has been reached! Next item will be from the end") : tr("End of json has been reached! Next item will be from the start"), treeview, QRect(100, 200, 11, 16), 3000);
            currentGotoIndexId = -1;
        }
    else if (((gotoIndexes_list.count() > 0 && currentGotoIndexId == -1) || (currentGotoIndexId > gotoIndexes_list.count() - 1)) && !directionForward)
        {
            currentGotoIndexId = gotoIndexes_list.count() - 1;
        }
    if (gotoIndexes_list.count() == 0  && model->itemFromIndex(model->index(0,0))->colorType()!=DiffColorType::None)
        {
            QRect widgetRect = find_lineEdit->contentsRect();
            QToolTip::showText(find_lineEdit->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), tr("<b><font \"color\"=green>Documents identical!</font></b>"), nullptr, QRect(100, 200, 11, 16), 3000);
        }
    else if (gotoIndexes_list.count() == 0  && model->itemFromIndex(model->index(0,0))->colorType()==DiffColorType::None)
        {
            QRect widgetRect = find_lineEdit->contentsRect();
            QToolTip::showText(find_lineEdit->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), tr("<b><font \"color\"=yellow>Start Comparison!</font></b>"), nullptr, QRect(100, 200, 11, 16), 3000);
        }
    if (currentGotoIndexId >= 0)
        {
            treeview->setCurrentIndex(gotoIndexes_list[currentGotoIndexId]);
            emit diffSelected(gotoIndexes_list[currentGotoIndexId]);
        }
}

void QJsonContainer::resetGoto()
{
    gotoIndexes_list.clear();
    currentGotoIndexId=-1;
}

void QJsonContainer::diffAmountUpdate()
{
    if(gotoIndexes_list.count()>0)
    {
        QPalette palette;
        palette.setColor(QPalette::Base,PREF_INST->diffColor(DiffColorType::Huge));
        diffAmount_lineEdit->setPalette(palette);
        diffAmount_lineEdit->setText(QString::number(gotoIndexes_list.count()));
    }
    else if(gotoIndexes_list.count()==0 && model->itemFromIndex(model->index(0,0))->colorType()!=DiffColorType::None)
    {
        QPalette palette;
        palette.setColor(QPalette::Base,PREF_INST->diffColor(DiffColorType::Identical));
        diffAmount_lineEdit->setPalette(palette);
        diffAmount_lineEdit->setText("0");
    }
    else
    {
        diffAmount_lineEdit->setPalette(gotDefaultPalette);
        diffAmount_lineEdit->setText("0");
    }
    diffAmount_lineEdit->setFixedWidth(diffAmount_lineEdit->text().size()*QFontMetrics(diffAmount_lineEdit->font()).maxWidth());
}

QList<QModelIndex> QJsonContainer::fillGotoList(QJsonModel *model, const QModelIndex &parent)
{
    QList<QModelIndex> retindex;
    int rowCount = model->rowCount(parent);

    for (int i = 0; i < rowCount; ++i)
        {
            QModelIndex idx0 = model->index(i, 0, parent);
            if (idx0.isValid())
                {
                    QJsonTreeItem *item=model->itemFromIndex(idx0);
                    DiffColorType bg=item->colorType();
                    if (bg!=DiffColorType::None && bg!=DiffColorType::Identical && (item->type()!=QJsonValue::Object && item->type()!=QJsonValue::Array))
                        {
                            retindex << idx0;
                        }
                    retindex << fillGotoList(model, idx0);
                }
        }

    return retindex;
}
