#include <QColorDialog>
#include <QDebug>
#include <QHeaderView>
#include <QShowEvent>
#include <QStyleFactory>

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "preferences.h"
#include "shortcutdelegate.h"


PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    connect(ui->listWidget, &QListWidget::currentRowChanged, ui->stackedWidget, &QStackedWidget::setCurrentIndex);
    connect(ui->alphaSpinBox, SIGNAL(valueChanged(int)), this, SLOT(alphaSpinBox_valueChanged(int)));
    connect(ui->restoreDefaultsPushButton, &QPushButton::clicked, this, &PreferencesDialog::restoreColorDefaultsPushButton_clicked);
    connect(ui->restoreShortcutsPushButton, &QPushButton::clicked, this, &PreferencesDialog::restoreShortcutDefaultsPushButton_clicked);
    connect(ui->tabsPosition_buttonGroup, &QButtonGroup::buttonClicked, this, &PreferencesDialog::tabsPosition_button_clicked);
    connect(ui->showJson_buttonGroup, &QButtonGroup::buttonClicked, this, &PreferencesDialog::showJsonButtonPosition_clicked);

    ui->shortcutsTableView->setItemDelegateForColumn(1, new ShortcutDelegate(this));

    ui->listWidget->setCurrentRow(0);

    m_colorMap[ui->hugeDiffPushButton] = &PREF_INST->hugeDiffColor;
    m_colorMap[ui->identicalDiffPushButton] = &PREF_INST->identicalDiffColor;
    m_colorMap[ui->moderateDiffPushButton] = &PREF_INST->moderateDiffColor;
    m_colorMap[ui->notPresentDiffButton] = &PREF_INST->notPresentDiffColor;
    m_colorMap[ui->keyWordPushButton] = &PREF_INST->syntaxKeywordColor;
    m_colorMap[ui->jsonKeysPushButton] = &PREF_INST->syntaxValueColor;

    QMap<QPushButton*, QColor*>::iterator i = m_colorMap.begin();
    while (i != m_colorMap.end()) {
        setupButton(i.key(), *i.value());
        connect(i.key(), &QPushButton::clicked,
                this, &PreferencesDialog::openColorDialog);
        ++i;
    }

    ui->alphaSpinBox->setValue(PREF_INST->diffColorsAlpha);

    //QTabWidget enums starts from zero, defult radiobox group enum -2
    if(ui->tabsPosition_buttonGroup->button((PREF_INST->tabsPosition)*(-1)-2))
    {
        ui->tabsPosition_buttonGroup->button((PREF_INST->tabsPosition)*(-1)-2)->setChecked(true);
    }
    else
    {
        PREF_INST->tabsPosition=0;
        ui->tabsPosition_buttonGroup->button(-2)->setChecked(true);
    }

    if(ui->showJson_buttonGroup->button(PREF_INST->showJsonButtonPosition))
    {
        ui->showJson_buttonGroup->button(PREF_INST->showJsonButtonPosition)->setChecked(true);
    }
    else
    {
        PREF_INST->showJsonButtonPosition=-2;
        ui->showJson_buttonGroup->button(-2)->setChecked(true);
    }

    // JSON Editing page - sync checkbox state from current prefs and
    // route toggles to a single slot that writes the right pref +
    // notifies listeners (MainWindow re-applies setEditable live).
    ui->singleTreeEditCheckBox->setChecked(PREF_INST->editableSingleTree);
    ui->diffViewEditCheckBox->setChecked(PREF_INST->editableDiffView);
    connect(ui->singleTreeEditCheckBox, &QCheckBox::toggled,
            this, &PreferencesDialog::editModeCheckBoxToggled);
    connect(ui->diffViewEditCheckBox,   &QCheckBox::toggled,
            this, &PreferencesDialog::editModeCheckBoxToggled);

    // Style page - combobox lists every QStyle Qt knows about plus a
    // "Default" sentinel that means "don't override." We block the
    // combo's signal while populating so the initial fill doesn't
    // count as a user pick.
    ui->appStyleComboBox->blockSignals(true);
    ui->appStyleComboBox->addItem(tr("Default"), QString());
    const QStringList styleKeys = QStyleFactory::keys();
    for (const QString &k : styleKeys)
        ui->appStyleComboBox->addItem(k, k);
    const int savedIdx = ui->appStyleComboBox->findData(PREF_INST->appStyle);
    ui->appStyleComboBox->setCurrentIndex(savedIdx >= 0 ? savedIdx : 0);
    ui->appStyleComboBox->blockSignals(false);
    connect(ui->appStyleComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::appStyleComboBox_currentIndexChanged);

    ui->useStyledTreeCheckBox->setChecked(PREF_INST->useStyledTree);
    connect(ui->useStyledTreeCheckBox, &QCheckBox::toggled,
            this, &PreferencesDialog::useStyledTreeCheckBox_toggled);
}

PreferencesDialog::~PreferencesDialog()
{
    PREF_INST->save();
    delete ui;
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    populateShortcutsTable();
}

void PreferencesDialog::populateShortcutsTable()
{
    qDebug() << "--- populateShortcutsTable() START ---";

    QStandardItemModel *model = new QStandardItemModel(Preferences::shortcutInfos.size(), 2, this);

    model->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut")});

    connect(model, &QStandardItemModel::itemChanged, this, &PreferencesDialog::shortcut_changed);

    int row = 0;
    for(const auto& info : Preferences::shortcutInfos)
    {
        QKeySequence shortcut = PREF_INST->shortcuts.value(info.key);
        qDebug() << "Populating row" << row << "for" << info.key << "with shortcut" << shortcut.toString();

        QStandardItem *actionItem = new QStandardItem(info.description);
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        actionItem->setData(QVariant::fromValue(info.key), Qt::UserRole);
        model->setItem(row, 0, actionItem);

        // Column 1: Shortcut
        QStandardItem *shortcutItem = new QStandardItem();
        shortcutItem->setData(QVariant::fromValue(shortcut), Qt::EditRole);
        shortcutItem->setData(shortcut.toString(QKeySequence::NativeText), Qt::DisplayRole);
        shortcutItem->setData(QVariant::fromValue(info.key), Qt::UserRole);
        model->setItem(row, 1, shortcutItem);

        row++;
    }

    ui->shortcutsTableView->setModel(model);
    ui->shortcutsTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->shortcutsTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    qDebug() << "--- populateShortcutsTable() END ---";
}

void PreferencesDialog::setupButton(QPushButton *b, const QColor &c)
{
    QSize s(64, 32);
    QPixmap pm(s);
    pm.fill(c);
    QIcon icon(pm);
    b->setIcon(icon);
    b->setIconSize(s);
}

void PreferencesDialog::openColorDialog()
{
    QPushButton *b = qobject_cast<QPushButton*>(sender());
    QColor *c = m_colorMap[b];

    QColorDialog dia(*c);
    if (dia.exec()) {
        *c = dia.selectedColor();
        setupButton(b, *c);
    }
}

void PreferencesDialog::alphaSpinBox_valueChanged(int val)
{
    PREF_INST->diffColorsAlpha = val;
}

void PreferencesDialog::restoreColorDefaultsPushButton_clicked()
{
    PREF_INST->restoreColorDefaults();

    ui->alphaSpinBox->setValue(PREF_INST->diffColorsAlpha);

    QMap<QPushButton*, QColor*>::iterator i = m_colorMap.begin();
    while (i != m_colorMap.end()) {
        setupButton(i.key(), *i.value());
        ++i;
    }
}

void PreferencesDialog::restoreShortcutDefaultsPushButton_clicked()
{
    PREF_INST->restoreShortcutDefaults();
    populateShortcutsTable();
}

void PreferencesDialog::tabsPosition_button_clicked(QAbstractButton* button)
{
       int id = ui->tabsPosition_buttonGroup->id(button);
       PREF_INST->tabsPosition=id*-1-2;
       PREF_INST->save();
}

void PreferencesDialog::showJsonButtonPosition_clicked(QAbstractButton* button)
{
       int id = ui->showJson_buttonGroup->id(button);
       PREF_INST->showJsonButtonPosition=id;
       PREF_INST->save();
}

void PreferencesDialog::editModeCheckBoxToggled(bool checked)
{
    Q_UNUSED(checked);   // we read both checkbox states fresh each time
    PREF_INST->editableSingleTree = ui->singleTreeEditCheckBox->isChecked();
    PREF_INST->editableDiffView   = ui->diffViewEditCheckBox->isChecked();
    PREF_INST->save();
    emit PREF_INST->editModeChanged();
}

void PreferencesDialog::appStyleComboBox_currentIndexChanged(int index)
{
    PREF_INST->appStyle = ui->appStyleComboBox->itemData(index).toString();
    PREF_INST->save();
}

void PreferencesDialog::useStyledTreeCheckBox_toggled(bool checked)
{
    PREF_INST->useStyledTree = checked;
    PREF_INST->save();
    emit PREF_INST->styledTreeChanged();
}

void PreferencesDialog::shortcut_changed(QStandardItem *item)
{
    if (item->column() == 1) {
        QString key = item->data(Qt::UserRole).toString();
        QKeySequence newShortcut = item->data(Qt::EditRole).value<QKeySequence>();
        PREF_INST->shortcuts[key] = newShortcut;
        item->setData(newShortcut.toString(QKeySequence::NativeText), Qt::DisplayRole);
        PREF_INST->save();
        qDebug() << "Shortcut changed for" << key << "to" << newShortcut.toString();
    }
}
