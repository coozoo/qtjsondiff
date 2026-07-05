/* Author: Yuriy Kuzin
 * Description: widget to load JSON from file or http source
 *              or you can paste it manualy into text area
 *              evrytime model will be updated
 *              it's possible to perform search in text view and json view
 */
#include "qjsoncontainer.h"
#include "jsonsyntaxhighlighter.h"
#include "jsonitemdelegate.h"

#include <QFileIconProvider>

#include <QDragEnterEvent>
#include <QMimeData>
#include <QTimer>
#include <QPainter>
#include <QKeyEvent>

#include "preferences/preferences.h"

static const QRect kTooltipRect(100, 200, 11, 16);
static const int kTooltipDuration = 3000;

QJsonContainer::QJsonContainer(QWidget *parent):
    QWidget(parent)
{

    QString expandSelectedRecursively_string = tr("Expand Selected Tree");
    QString collapseSelectedRecursively_string = tr("Collapse Selected Tree");

    copyRow = myMenu.addAction(tr("Copy Row"));
    copyRows = myMenu.addAction(tr("Copy all Rows"));
    copyPath = myMenu.addAction(tr("Copy Path"));
    copyJqPath = myMenu.addAction(tr("Copy jq Path"));
    copyJsonPlainText = myMenu.addAction(tr("Copy Plain JSON"));
    copyJsonPrettyText = myMenu.addAction(tr("Copy Pretty JSON"));
    copySelectedJsonValuePlain = myMenu.addAction(tr("Copy Plain selected JSON value"));
    copyJsonByPath = myMenu.addAction(tr("Copy Pretty selected JSON"));
    myMenu.addSeparator();
    singleExpandSelectedRecursively = myMenu.addAction(expandSelectedRecursively_string);
    singleCollapseSelectedRecursively = myMenu.addAction(collapseSelectedRecursively_string);
    myMenu.addSeparator();
    // Edit-mode-only — visibility flipped on/off in showContextMenu.
    addChildAction  = myMenu.addAction(tr("Add child"));
    deleteRowAction = myMenu.addAction(tr("Delete row"));

    expandSelectedRecursively = multiSelectMenu.addAction(expandSelectedRecursively_string);
    collapseSelectedRecursively = multiSelectMenu.addAction(collapseSelectedRecursively_string);
    expandSelected = multiSelectMenu.addAction(tr("Expand Selected"));
    collapseSelected = multiSelectMenu.addAction(tr("Collapse Selected"));

    copyRow->setShortcutContext(Qt::WidgetShortcut);
    copyRows->setShortcutContext(Qt::WidgetShortcut);
    copyPath->setShortcutContext(Qt::WidgetShortcut);
    copyJqPath->setShortcutContext(Qt::WidgetShortcut);
    copyJsonPlainText->setShortcutContext(Qt::WidgetShortcut);
    copyJsonPrettyText->setShortcutContext(Qt::WidgetShortcut);
    copySelectedJsonValuePlain->setShortcutContext(Qt::WidgetShortcut);
    copyJsonByPath->setShortcutContext(Qt::WidgetShortcut);
    singleExpandSelectedRecursively->setShortcutContext(Qt::WidgetShortcut);
    singleCollapseSelectedRecursively->setShortcutContext(Qt::WidgetShortcut);
    expandSelected->setShortcutContext(Qt::WidgetShortcut);
    collapseSelected->setShortcutContext(Qt::WidgetShortcut);
    expandSelectedRecursively->setShortcutContext(Qt::WidgetShortcut);
    collapseSelectedRecursively->setShortcutContext(Qt::WidgetShortcut);


    addAction(copyRow);
    addAction(copyRows);
    addAction(copyPath);
    addAction(copyJqPath);
    addAction(copyJsonPlainText);
    addAction(copyJsonPrettyText);
    addAction(copySelectedJsonValuePlain);
    addAction(copyJsonByPath);
    addAction(singleExpandSelectedRecursively);
    addAction(singleCollapseSelectedRecursively);
    addAction(expandSelected);
    addAction(collapseSelected);
    addAction(expandSelectedRecursively);
    addAction(collapseSelectedRecursively);


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
    treeview->setItemDelegateForColumn(1, new JsonItemDelegate(this));
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

    // Thin progress bar directly under the search input. Fires while
    // findModelText is walking the tree; goes idle once the index is
    // built — also serves as a visual "search is ready" cue.
    mSearchProgress = new QProgressBar();
    mSearchProgress->setMaximumHeight(3);
    mSearchProgress->setTextVisible(false);
    mSearchProgress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mSearchProgress->hide();

    // Wrap line-edit + progress bar in a compact widget so the toolbar
    // can host them as one item. Zero margins/spacing so the bar hugs
    // the line-edit's bottom edge.
    auto *findWrapper = new QWidget();
    auto *findWrapperLayout = new QVBoxLayout(findWrapper);
    findWrapperLayout->setContentsMargins(0, 0, 0, 0);
    findWrapperLayout->setSpacing(0);
    findWrapperLayout->addWidget(find_lineEdit);
    findWrapperLayout->addWidget(mSearchProgress);
    findNext_toolbutton = new QAction(toolbar);
    QIcon findNext_icon = QIcon(createPixmapFromText(tr(">>")));
    findNext_toolbutton->setText(tr("Search Next"));
    findNext_toolbutton->setIcon(findNext_icon);
    findNext_toolbutton->setToolTip(tr("Find Next"));
    findPrevious_toolbutton = new QAction(toolbar);
    QIcon findPrevious_icon = QIcon(createPixmapFromText(tr("<<")));
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
    toolbar->addWidget(findWrapper);
    toolbar->addAction(findPrevious_toolbutton);
    toolbar->addAction(findNext_toolbutton);
    toolbar->addAction(findCaseSensitivity_toolbutton);
    toolbar->addSeparator();
    toolbar->setIconSize(QSize(28, 28));


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

    // Optional custom QSS for the tree view. Off by default — matches
    // the pre-Style-prefs look (plain platform QTreeView). When
    // Preferences::useStyledTree is on we load qss/qjsontreeview.qss
    // (custom branch icons + hover/select gradients). Re-applied live
    // whenever the user flips the checkbox in Preferences.
    auto applyStyledTree = [this]() {
        if (PREF_INST->useStyledTree)
            {
                QFile file(":/qss/qjsontreeview.qss");
                if (file.open(QIODevice::ReadOnly))
                    {
                        treeview->setStyleSheet(
                            QString::fromUtf8(file.readAll()));
                        file.close();
                    }
            }
        else
            {
                treeview->setStyleSheet(QString());
            }
        treeview->ensurePolished();
    };
    applyStyledTree();
    connect(PREF_INST, &Preferences::styledTreeChanged,
            this, applyStyledTree);
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

    // Populate the action map for ALL shortcuts
    m_shortcutActionMap["copy_row"] = copyRow;
    m_shortcutActionMap["copy_all_rows"] = copyRows;
    m_shortcutActionMap["copy_path"] = copyPath;
    m_shortcutActionMap["copy_jq_path"] = copyJqPath;
    m_shortcutActionMap["copy_json_plain_text"] = copyJsonPlainText;
    m_shortcutActionMap["copy_json_pretty_text"] = copyJsonPrettyText;
    m_shortcutActionMap["copy_selected_pretty"] = copyJsonByPath;
    m_shortcutActionMap["copy_selected_plain"] = copySelectedJsonValuePlain;

    applyShortcuts(); // Apply on startup

    //connect right click(context menu) signal/slot
    connect(treeview, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
    connect(treeview, SIGNAL(expanded(const QModelIndex &)), this, SLOT(on_treeview_item_expanded()));
    //connect(expandAll_Checkbox, SIGNAL(stateChanged(int)), this, SLOT(on_expandAll_checkbox_marked()));
    connect(expandAll_Checkbox, &QAction::triggered, this, &QJsonContainer::on_expandAll_checkbox_marked);
    connect(browse_toolButton, SIGNAL(clicked()), this, SLOT(on_browse_toolButton_clicked()));
    connect(refresh_toolButton, &QToolButton::clicked, this, &QJsonContainer::on_refresh_toolButton_clicked);
    //connect(sortObj_toolButton, SIGNAL(clicked()), this, SLOT(on_sortObj_toolButton_clicked()));
    connect(sortObj_toolButton, &QAction::triggered, this, &QJsonContainer::on_sortObj_toolButton_clicked);
    connect(switchview_action, &QAction::triggered, showjson_pushbutton, &QPushButton::click);
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
    connect(model, &QJsonModel::modelChanged, this, &QJsonContainer::on_model_changed);

    connect(copyRow, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copyRows, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copyPath, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copyJqPath, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copyJsonPlainText, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copyJsonPrettyText, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copySelectedJsonValuePlain, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(copyJsonByPath, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(singleExpandSelectedRecursively, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(singleCollapseSelectedRecursively, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(expandSelected, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(collapseSelected, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(expandSelectedRecursively, &QAction::triggered, this, &QJsonContainer::onActionTriggered);
    connect(collapseSelectedRecursively, &QAction::triggered, this, &QJsonContainer::onActionTriggered);

    connect(PREF_INST, &Preferences::shortcutsUpdated, this, &QJsonContainer::applyShortcuts);
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

// yeah it is duplicated logic for menu action basically i don't want to refactor it now
void QJsonContainer::onActionTriggered()
{
    qDebug() << "onActionTriggered() called.";

    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        {
            qDebug() << "  ERROR: sender() is not a QAction.";
            return;
        }

    qDebug() << "  Action triggered:" << action->text();

    QModelIndex idx = treeview->selectionModel()->currentIndex();
    if (!idx.isValid())
        {
            qDebug() << "  ERROR: treeview->selectionModel()->currentIndex() is INVALID. No item selected or treeview not focused.";
            return;
        }

    qDebug() << "  Index is VALID. Row:" << idx.row() << "Column:" << idx.column();

    QClipboard *clip = QApplication::clipboard();

    if (action == copyRow)
        {
            qDebug() << "    Executing: copyRow";
            QStringList strings = extractItemTextFromModel(idx);
            clip->setText(strings.join("\n"));
            qDebug() << "    Copied to clipboard:" << clip->text();
        }
    else if (action == copyRows)
        {
            qDebug() << "    Executing: copyRows";
            QString string = extractStringsFromModel(model, QModelIndex()).join("\n");
            clip->setText(string);
            qDebug() << "    Copied to clipboard:" << clip->text();
        }
    else if (action == copyPath)
        {
            qDebug() << "    Executing: copyPath";
            QString string = model->jsonPath(idx);
            clip->setText(string);
            qDebug() << "    Copied to clipboard:" << clip->text();
        }
    else if (action == copyJqPath)
        {
            qDebug() << "    Executing: copyJqPath";
            QString string = this->JsonPathToJq(model->jsonPath(idx));
            clip->setText(string);
            qDebug() << "    Copied to clipboard:" << clip->text();
        }
    else if (action == copyJsonPlainText)
        {
            qDebug() << "    Executing: copyJsonPlainText";
            clip->setText(QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8()).toJson(QJsonDocument::Compact));
            qDebug() << "    Copied Plain JSON to clipboard.";
        }
    else if (action == copyJsonPrettyText)
        {
            qDebug() << "    Executing: copyJsonPrettyText";
            clip->setText(QJsonDocument::fromJson(viewjson_plaintext->toPlainText().toUtf8()).toJson(QJsonDocument::Indented));
            qDebug() << "    Copied Pretty JSON to clipboard.";
        }
    else if (action == copyJsonByPath)
        {
            qDebug() << "    Executing: copyJsonByPath";
            QJsonDocument doc = QJsonDocument::fromJson(getJson(model->jsonIndexPath(idx)).toUtf8());
            clip->setText(doc.toJson(QJsonDocument::Indented));
            qDebug() << "    Copied selected JSON to clipboard.";
        }
    else if (action == copySelectedJsonValuePlain)
        {
            qDebug() << "    Executing: copySelectedJsonValuePlain";
            QJsonDocument doc = QJsonDocument::fromJson(getJson(model->jsonIndexPath(idx)).toUtf8());
            clip->setText(doc.toJson(QJsonDocument::Compact));
            qDebug() << "    Copied selected plain JSON to clipboard.";
        }
    else if (action == singleExpandSelectedRecursively)
        {
            qDebug() << "    Executing: singleExpandSelectedRecursively";
            expandRecursively(idx, treeview, true);
        }
    else if (action == singleCollapseSelectedRecursively)
        {
            qDebug() << "    Executing: singleCollapseSelectedRecursively";
            expandRecursively(idx, treeview, false);
        }
    else if (action == expandSelected)
        {
            qDebug() << "    Executing: expandSelected";
            QModelIndexList selection = treeview->selectionModel()->selectedRows();
            for (const QModelIndex &index : selection)
                {
                    treeview->setExpanded(index, true);
                }
        }
    else if (action == collapseSelected)
        {
            qDebug() << "    Executing: collapseSelected";
            QModelIndexList selection = treeview->selectionModel()->selectedRows();
            for (const QModelIndex &index : selection)
                {
                    treeview->setExpanded(index, false);
                }
        }
    else if (action == expandSelectedRecursively)
        {
            qDebug() << "    Executing: expandSelectedRecursively";
            QModelIndexList selection = treeview->selectionModel()->selectedRows();
            for (const QModelIndex &index : selection)
                {
                    expandRecursively(index, treeview, true);
                }
        }
    else if (action == collapseSelectedRecursively)
        {
            qDebug() << "    Executing: collapseSelectedRecursively";
            QModelIndexList selection = treeview->selectionModel()->selectedRows();
            for (const QModelIndex &index : selection)
                {
                    expandRecursively(index, treeview, false);
                }
        }
}

void QJsonContainer::applyShortcuts()
{
    for (auto it = m_shortcutActionMap.constBegin(); it != m_shortcutActionMap.constEnd(); ++it)
        {
            it.value()->setShortcut(PREF_INST->shortcuts.value(it.key()));
        }
}

void QJsonContainer::on_model_changed()
{
    if (model && !model->hasParseError())
        {
            viewjson_plaintext->setPlainText(model->getJsonDocument().toJson(QJsonDocument::Indented));
        }
}

void QJsonContainer::setEditable(bool editable)
{
    mIsEditable = editable;
    model->setEditable(editable);

    // Tree-level item DnD is on iff the model is editable. Both
    // directions (drag-out, drop-in) are gated together. The file-URL
    // drop on the surrounding groupbox is untouched — the tree's
    // default dragEnter handler will ignore MIME types not advertised
    // by the model (we only advertise our custom type), so file drags
    // propagate through to the groupbox event filter unchanged.
    if (editable)
    {
        treeview->setDragEnabled(true);
        treeview->setAcceptDrops(true);
        treeview->setDropIndicatorShown(true);
        treeview->setDragDropMode(QAbstractItemView::DragDrop);
        treeview->setDefaultDropAction(Qt::CopyAction);
    }
    else
    {
        treeview->setDragEnabled(false);
        treeview->setAcceptDrops(false);
        treeview->setDropIndicatorShown(false);
        treeview->setDragDropMode(QAbstractItemView::NoDragDrop);
    }
}

QModelIndex QJsonContainer::addChildOf(const QModelIndex &parent)
{
    if (!mIsEditable) return QModelIndex();

    // Invalid parent → root. itemFromIndex returns nullptr for invalid
    // indexes, so the type / child-count probes go through rootType()
    // and rowCount() which both treat invalid as root automatically.
    QJsonValue::Type parentType;
    if (parent.isValid())
    {
        QJsonTreeItem *p = model->itemFromIndex(parent);
        if (!p) return QModelIndex();
        parentType = p->type();
    }
    else
    {
        parentType = model->rootType();
    }
    if (parentType != QJsonValue::Object && parentType != QJsonValue::Array)
        return QModelIndex();

    // Default key. Arrays auto-key by position inside
    // QJsonModel::appendChildFromJson, so any value works here; objects
    // need a unique key so we don't immediately collide with a sibling
    // (QJsonDocument would de-dupe on serialize, hiding the new entry).
    QString key;
    if (parentType == QJsonValue::Object)
    {
        QSet<QString> used;
        const int n = model->rowCount(parent);
        for (int i = 0; i < n; ++i)
        {
            QModelIndex childIdx = model->index(i, 0, parent);
            QJsonTreeItem *c = model->itemFromIndex(childIdx);
            if (c) used.insert(c->key());
        }
        key = "new_key";
        for (int k = 1; used.contains(key); ++k)
            key = QString("new_key_%1").arg(k);
    }

    QModelIndex newIdx = model->appendChildFromJson(parent, key,
                                                   QJsonValue(QString()));
    if (!newIdx.isValid()) return newIdx;

    // Surface the new row: expand the parent, select the new child,
    // start in-place editing on the most useful column.
    if (parent.isValid())
        treeview->expand(parent);
    treeview->setCurrentIndex(newIdx);
    treeview->scrollTo(newIdx);
    // Objects: edit the key (so the user names it). Arrays: keys are
    // positional, jump straight to the value column.
    const int col = (parentType == QJsonValue::Array) ? 2 : 0;
    treeview->edit(newIdx.siblingAtColumn(col));
    return newIdx;
}

bool QJsonContainer::removeAt(const QModelIndex &target)
{
    if (!mIsEditable) return false;
    return model->removeRowAt(target);
}

void QJsonContainer::showGoto(bool show)
{
    //no way to hide :)
    if (show)
        {
            goToNextDiff_toolbutton = new QAction(toolbar);
            QIcon goToNextDiff_icon = QIcon(createPixmapFromText(tr("->|")));
            goToNextDiff_toolbutton->setIcon(goToNextDiff_icon);
            goToNextDiff_toolbutton->setText(tr("Go to Next Diff"));
            goToNextDiff_toolbutton->setToolTip(tr("Go to Next Diff"));
            goToPreviousDiff_toolbutton = new QAction(toolbar);
            QIcon goToPreviousDiff_icon = QIcon(createPixmapFromText(tr("|<-")));
            goToPreviousDiff_toolbutton->setIcon(goToPreviousDiff_icon);
            goToPreviousDiff_toolbutton->setText(tr("Go to Previous Diff"));
            goToPreviousDiff_toolbutton->setToolTip(tr("Go to Previous Diff"));
            diffAmount_lineEdit = new QLineEdit(toolbar);
            diffAmount_lineEdit->setText("0");
            diffAmount_lineEdit->setToolTip(tr("Amount of Differences (without root objects/arrays, means values only)"));
            diffAmount_lineEdit->setReadOnly(true);
            diffAmount_lineEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
            diffAmount_lineEdit->setFixedWidth(QFontMetrics(diffAmount_lineEdit->font()).maxWidth());
            diffAmount_lineEdit->setAlignment(Qt::AlignCenter);
            gotDefaultPalette = diffAmount_lineEdit->palette();
            toolbar->addSeparator();
            toolbar->addWidget(diffAmount_lineEdit);
            toolbar->addAction(goToPreviousDiff_toolbutton);
            toolbar->addAction(goToNextDiff_toolbutton);
            toolbar->addSeparator();
            connect(goToPreviousDiff_toolbutton, &QAction::triggered, this, &QJsonContainer::on_GoToPreviousDiff_toolbutton_clicked);
            connect(goToNextDiff_toolbutton, &QAction::triggered, this, &QJsonContainer::on_GoToNextDiff_toolbutton_clicked);
        }
}

QPixmap QJsonContainer::createPixmapFromText(const QString &text)
{
    int iconSize = toolbar->iconSize().width();
    QPixmap pix(iconSize, iconSize);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);

    QFont font("Times", iconSize / 2, QFont::Bold);
    painter.setFont(font);

    QRect rect = pix.rect();
    QFontMetrics fm(font);
    while (fm.horizontalAdvance(text) > iconSize - 4 && font.pointSize() > 1)
        {
            font.setPointSize(font.pointSize() - 1);
            painter.setFont(font);
            fm = QFontMetrics(font);
        }

    painter.setPen(QPen(Qt::black));
    painter.drawText(rect, Qt::AlignCenter, text);
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
    QPoint globalPos = treeview->mapToGlobal(point);
    QModelIndex idx = treeview->indexAt(point);

    bool isContainer = false;
    if (idx.isValid())
        {
            QJsonTreeItem *item = static_cast<QJsonTreeItem *>(idx.internalPointer());
            QJsonValue::Type type = item->type();
            isContainer = (type == QJsonValue::Object || type == QJsonValue::Array);
        }
    copyJsonByPath->setEnabled(isContainer);
    copySelectedJsonValuePlain->setEnabled(isContainer);

    // Add/Delete row: shown only in editable mode. "Add child" enables
    // when the click target is a container (or empty space → root,
    // which is always a container after loadJson). "Delete row"
    // enables on any non-root row.
    if (addChildAction)
    {
        addChildAction->setVisible(mIsEditable);
        addChildAction->setEnabled(!idx.isValid() || isContainer);
    }
    if (deleteRowAction)
    {
        deleteRowAction->setVisible(mIsEditable);
        deleteRowAction->setEnabled(idx.isValid() && idx.parent().isValid());
    }

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
            else if (selectedItem == copyJqPath)
                {
                    QModelIndex idx = treeview->indexAt(point);
                    QString string = this->JsonPathToJq(model->jsonPath(idx));
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
                    qDebug() << "copyJsonByPath";
                    QModelIndex idx = treeview->indexAt(point);
                    QJsonDocument doc = QJsonDocument::fromJson(getJson(model->jsonIndexPath(idx)).toUtf8());
                    clip->setText(doc.toJson(QJsonDocument::Indented));
                }
            else if (selectedItem == copySelectedJsonValuePlain)
                {
                    qDebug() << "copySelectedJsonValuePlain";
                    QModelIndex idx = treeview->indexAt(point);
                    QJsonDocument doc = QJsonDocument::fromJson(getJson(model->jsonIndexPath(idx)).toUtf8());
                    clip->setText(doc.toJson(QJsonDocument::Compact));
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
            else if (selectedItem == addChildAction)
                {
                    // Empty-space click resolves to the root container
                    // via the invalid index → itemFromIndex(QModelIndex())
                    // path inside addChildOf.
                    addChildOf(idx);
                }
            else if (selectedItem == deleteRowAction)
                {
                    removeAt(idx);
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

QString QJsonContainer::JsonPathToJq(const QString &qtPath) const
{
    QString jqPath = ".";
    QStringList parts = qtPath.split("->");
    QString prevType;
    QRegularExpression validKey("^[A-Za-z_][A-Za-z0-9_]*$");

    for (const QString &part : parts)
        {
            int p = part.indexOf('(');
            QString key = p > 0 ? part.left(p) : part;
            QString type = p > 0 ? part.mid(p + 1, part.size() - p - 2) : "";

            if (key == "root" || key.isEmpty()) { prevType = type; continue; }

            bool isInt = false;
            int idx = key.toInt(&isInt);

            if (prevType == "Array" && isInt && key == QString::number(idx))
                {
                    jqPath += "[" + key + "]";
                }
            else if (validKey.match(key).hasMatch())
                {
                    jqPath += (jqPath == "." ? "" : ".") + key;
                }
            else
                {
                    jqPath += "[\"" + key + "\"]";
                }
            prevType = type;
        }
    return "'" + jqPath + "'";
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
    // Same show/hide bracket as findTextJsonIndexHandler — the case-
    // sensitivity toggle re-runs findModelText from scratch, so the
    // user should get the same visual "indexing" cue.
    const int totalNodes = countTreeNodes(model, QModelIndex());
    mSearchProgressCounter = 0;
    if (mSearchProgress)
    {
        mSearchProgress->setRange(0, totalNodes > 0 ? totalNodes : 1);
        mSearchProgress->setValue(0);
        mSearchProgress->show();
    }
    currentFindIndexesList = findModelText(model, QModelIndex());
    if (mSearchProgress)
    {
        mSearchProgress->setValue(mSearchProgress->maximum());
        mSearchProgress->hide();
    }
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
            // Show the thin progress bar under the search input while
            // findModelText walks the tree. Determinate range = total
            // node count so the fill correlates with actual progress.
            const int totalNodes = countTreeNodes(model, QModelIndex());
            mSearchProgressCounter = 0;
            if (mSearchProgress)
            {
                mSearchProgress->setRange(0, totalNodes > 0 ? totalNodes : 1);
                mSearchProgress->setValue(0);
                mSearchProgress->show();
            }
            currentFindIndexesList = findModelText(model, QModelIndex());
            if (mSearchProgress)
            {
                mSearchProgress->setValue(mSearchProgress->maximum());
                mSearchProgress->hide();
            }
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
            QToolTip::showText(treeview->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), (!direction) ? tr("Start of json has been reached! Next item will be from the end") : tr("End of json has been reached! Next item will be from the start"), treeview, kTooltipRect, kTooltipDuration);
            currentFindIndexId = -1;
        }
    else if (((currentFindIndexesList.count() > 0 && currentFindIndexId == -1) || (currentFindIndexId > currentFindIndexesList.count() - 1)) && !direction)
        {
            currentFindIndexId = currentFindIndexesList.count() - 1;
        }
    if (currentFindIndexesList.count() == 0)
        {
            QRect widgetRect = find_lineEdit->contentsRect();
            QToolTip::showText(find_lineEdit->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), tr("<b><font \"color\"=red>Text Not Found!</font></b>"), nullptr, kTooltipRect, kTooltipDuration);
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
            if (!model->hasParseError())
                {
                    QJsonDocument doc = model->getJsonDocument();
                    viewjson_plaintext->setPlainText(doc.toJson(QJsonDocument::Indented));
                }
            viewjson_plaintext->setVisible(true);
            treeview->setVisible(false);
            showjson_pushbutton->setText(tr("Show Json View"));
            if (model->hasParseError())
                {
                    int errorOffset = model->lastErrorOffset();
                    QTextCursor cursor = viewjson_plaintext->textCursor();
                    cursor.setPosition(errorOffset);
                    viewjson_plaintext->setTextCursor(cursor);
                    viewjson_plaintext->setFocus();
                }
        }
    emit jsonUpdated();
}

void QJsonContainer::on_browse_toolButton_clicked()
{
    QString dialogTitle = tr("Open File");
    QString dialogFilter = tr("Files (*.json *.jsn *.txt);;All Files (*)");
    QString dialogDir = "";
    if (!filePath_lineEdit->text().isEmpty())
        {
            dialogDir = filePath_lineEdit->text();
        }
    QString fileName = QFileDialog::getOpenFileName(this, dialogTitle, dialogDir, dialogFilter);
    if (!fileName.isEmpty())
        {
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
    loadJson(datastr);
}

void QJsonContainer::loadJson(const QString &data)
{
    QByteArray utf8Data = data.toUtf8();
    model->loadJson(utf8Data);
    if (model->hasParseError())
        {
            // Do not update text view if error
            QTimer::singleShot(100, [this]()
            {
                QToolTip::showText(this->mapToGlobal(QPoint(0, 0)),
                                   tr("<b><font color='red'>JSON parse error:</font></b> ") + model->lastErrorMessage(),
                                   this, kTooltipRect, kTooltipDuration);
            });
            return;
        }
    viewjson_plaintext->setPlainText((QJsonDocument::fromJson(utf8Data)).toJson(QJsonDocument::Indented));
    on_expandAll_checkbox_marked();
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
    QJsonDocument resultData = data;
    if (data.isObject())
        {
            QJsonObject jsonObj = data.object();
            resultData = QJsonDocument::fromVariant(sortObjectArraysGrabObject(jsonObj).toVariantMap());
        }
    else
        {
            QJsonArray jsonArr = data.array();
            resultData = QJsonDocument::fromVariant(sortObjectArraysGrabArray(jsonArr).toVariantList());
        }
    return resultData;
}
//Count "weight of string" or it's rather some kind of hashcode
//from my point of view it's almost unique and possible to recognize identical string
//"weight of string"="sum charachters code numbers" * "string length"
int QJsonContainer::countStringWeight(const QString &inStr)
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
QJsonArray QJsonContainer::sortObjectArraysGrabArray(QJsonArray data)
{
    QJsonArray resultData;
    if (data.at(0).isObject())
        {
            //qDebug()<<data;
            //qDebug()<<data.count();
            std::sort(data.begin(), data.end(), wayToSort);

            //qDebug()<<data;

        }
    else if (data.at(0).isDouble())
        {
            //qDebug()<<data;
            //qDebug()<<data.count();
            std::sort(data.begin(), data.end(), wayToSort);
            //qDebug()<<data;

        }
    resultData = data;
    for (int arrayId = 0; arrayId < resultData.size(); ++arrayId)
        {
            const QJsonValue &value = resultData[arrayId];
            if (value.isObject())
                {
                    // qDebug() << "Object" << value.toString();
                    resultData.removeAt(arrayId);
                    resultData.insert(arrayId, sortObjectArraysGrabObject(value.toObject()));
                }
        }
    //qDebug()<<resultData;
    return resultData;
}

QJsonObject QJsonContainer::sortObjectArraysGrabObject(QJsonObject data)
{
    QJsonObject resultData = data;
    for (QJsonObject::const_iterator iter = data.begin(); iter != data.end(); ++iter)
        {
            if (iter.value().isArray())
                {
                    //qDebug() << iter.key()  << "Array" << iter.value().toString();
                    resultData.remove(iter.key());
                    resultData.insert(iter.key(), sortObjectArraysGrabArray(iter.value().toArray()));
                }
            else if (iter.value().isObject())
                {
                    //qDebug() << iter.key() << "Object" << iter.value().toString();
                    resultData.remove(iter.key());
                    resultData.insert(iter.key(), sortObjectArraysGrabObject(iter.value().toObject()));

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
                    [[fallthrough]];
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

int QJsonContainer::countTreeNodes(QJsonModel *model, const QModelIndex &parent) const
{
    if (!model) return 0;
    int total = 0;
    const int n = model->rowCount(parent);
    for (int i = 0; i < n; ++i)
    {
        QModelIndex idx = model->index(i, 0, parent);
        total += 1 + countTreeNodes(model, idx);
    }
    return total;
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
                    QJsonTreeItem *item = static_cast<QJsonTreeItem*>(idx0.internalPointer());
                    if (item && item->isPhantom())
                        continue;
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
                    // Progress tick per visited node. Every 200 nodes we
                    // update the bar and drain user-events-free ticks
                    // through the event loop so the bar can repaint mid-
                    // walk. ExcludeUserInputEvents keeps a stray click
                    // from re-entering the search handler while we're
                    // still building the index.
                    ++mSearchProgressCounter;
                    if (mSearchProgress && mSearchProgress->isVisible()
                            && (mSearchProgressCounter % 200) == 0)
                    {
                        mSearchProgress->setValue(mSearchProgressCounter);
                        QCoreApplication::processEvents(
                            QEventLoop::ExcludeUserInputEvents);
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
    if (diffAmount_lineEdit)
        {
            diffAmountUpdate();
        }
}

bool QJsonContainer::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj)
    if (event->type() == QEvent::ShortcutOverride)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            QKeySequence sequence(keyEvent->key() | keyEvent->modifiers());

            for (auto it = m_shortcutActionMap.constBegin(); it != m_shortcutActionMap.constEnd(); ++it)
                {
                    QAction *action = it.value();
                    if (!action->shortcut().isEmpty() && action->shortcut() == sequence)
                        {
                            qDebug() << "ShortcutOverride match for action:" << action->text();
                            action->trigger();
                            event->accept();
                            return true;
                        }
                }
        }
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
    if (event->type() == QEvent::DragEnter)
        {
            QDragEnterEvent *dropEvent = static_cast<QDragEnterEvent *>(event);

            qDebug() << "drag enter";
            qDebug() << dropEvent->mimeData()->urls();
            if (dropEvent->mimeData()->hasUrls())
                {
                    QStringList pathList;
                    QList<QUrl> urlList = dropEvent->mimeData()->urls();

                    for (int i = 0; i < urlList.size() && i < 32; ++i)
                        {
                            pathList.append(urlList.at(i).toLocalFile());
                        }

                    if (pathList.count() > 0)
                        {
                            dropEvent->acceptProposedAction();
                            qDebug() << pathList;
                        }

                }
        }
    if (event->type() == QEvent::Drop)
        {
            qDebug() << "drop";
            QDropEvent *dropEvent = static_cast<QDropEvent *>(event);
            qDebug() << dropEvent->mimeData()->hasUrls();
            if (dropEvent->mimeData()->hasUrls())
                {
                    QStringList pathList;
                    QList<QUrl> urlList = dropEvent->mimeData()->urls();

                    for (int i = 0; i < urlList.size() && i < 32; ++i)
                        {
                            pathList.append(urlList.at(i).toLocalFile());
                        }

                    if (pathList.count() > 0)
                        {
                            loadJsonFile(pathList[0]);
                        }

                }

        }
    qDebug() << event->type();
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
            qDebug() << "bottom";
//            if(toolbarbutton)
//            {
//                toolbar->removeAction(toolbarbutton);
//            }
//            showjson_pushbutton->setParent(treeview_groupbox);
//            treeview_layout->addWidget(showjson_pushbutton);
            showjson_pushbutton->show();
            break;
        case -3:
            qDebug() << "top inline";
            //treeview_layout->removeWidget(showjson_pushbutton);
//            showjson_pushbutton->setParent(toolbar);
            //toolbarbutton=toolbar->insertWidget(toolbar->actionAt(1,0),showjson_pushbutton);
//            toolbarbutton=toolbar->addWidget(showjson_pushbutton);
            showjson_pushbutton->hide();
            break;
        default:
            qDebug() << "default";
        }
}

void QJsonContainer::on_GoToNextDiff_toolbutton_clicked()
{

    qDebug() << gotoIndexes_list;
    gotoIndexHandler(true);

}

void QJsonContainer::on_GoToPreviousDiff_toolbutton_clicked()
{

    qDebug() << gotoIndexes_list;
    gotoIndexHandler(false);
}

//another one crap handler :)

void QJsonContainer::gotoIndexHandler(bool directionForward)
{
    QModelIndex idx = treeview->currentIndex();
    if (idx.isValid())
        {
            if (idx.column() != 0)
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
            QToolTip::showText(treeview->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), (!directionForward) ? tr("Start of json has been reached! Next item will be from the end") : tr("End of json has been reached! Next item will be from the start"), treeview, kTooltipRect, kTooltipDuration);
            currentGotoIndexId = -1;
        }
    else if (((gotoIndexes_list.count() > 0 && currentGotoIndexId == -1) || (currentGotoIndexId > gotoIndexes_list.count() - 1)) && !directionForward)
        {
            currentGotoIndexId = gotoIndexes_list.count() - 1;
        }
    const QJsonTreeItem *item = model->itemFromIndex(model->index(0, 0));
    if (item)
        {
            if (gotoIndexes_list.count() == 0  && model->itemFromIndex(model->index(0, 0))->colorType() != DiffColorType::None)
                {
                    QRect widgetRect = find_lineEdit->contentsRect();
                    QToolTip::showText(find_lineEdit->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), tr("<b><font \"color\"=green>Documents identical!</font></b>"), nullptr, kTooltipRect, kTooltipDuration);
                }
            else if (gotoIndexes_list.count() == 0  && model->itemFromIndex(model->index(0, 0))->colorType() == DiffColorType::None)
                {
                    QRect widgetRect = find_lineEdit->contentsRect();
                    QToolTip::showText(find_lineEdit->mapToGlobal(QPoint(widgetRect.center().x(), widgetRect.center().y())), tr("<b><font \"color\"=yellow>Start Comparison!</font></b>"), nullptr, kTooltipRect, kTooltipDuration);
                }
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
    currentGotoIndexId = -1;
}

void QJsonContainer::diffAmountUpdate()
{
    if (gotoIndexes_list.count() > 0)
        {
            QPalette palette;
            palette.setColor(QPalette::Base, PREF_INST->diffColor(DiffColorType::Huge));
            diffAmount_lineEdit->setPalette(palette);
            diffAmount_lineEdit->setText(QString::number(gotoIndexes_list.count()));
        }
    else if (gotoIndexes_list.count() == 0)
        {
            QJsonTreeItem *item = model->itemFromIndex(model->index(0, 0));
            if (item && item->colorType() != DiffColorType::None)
                {
                    QPalette palette;
                    palette.setColor(QPalette::Base, PREF_INST->diffColor(DiffColorType::Identical));
                    diffAmount_lineEdit->setPalette(palette);
                    diffAmount_lineEdit->setText("0");
                }
            else
                {
                    diffAmount_lineEdit->setPalette(gotDefaultPalette);
                    diffAmount_lineEdit->setText("0");
                }
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
                    QJsonTreeItem *item = model->itemFromIndex(idx0);
                    if (item && item->isPhantom())
                        continue;
                    DiffColorType bg = item->colorType();
                    if (bg != DiffColorType::None && bg != DiffColorType::Identical && (item->type() != QJsonValue::Object && item->type() != QJsonValue::Array))
                        {
                            retindex << idx0;
                        }
                    retindex << fillGotoList(model, idx0);
                }
        }

    return retindex;
}
