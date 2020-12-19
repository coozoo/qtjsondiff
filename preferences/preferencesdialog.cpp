#include <QColorDialog>

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "preferences.h"


PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    connect(ui->listWidget, &QListWidget::currentRowChanged,
            ui->stackedWidget, &QStackedWidget::setCurrentIndex);
    // There are 2 signals - int and QString - we cannot enforce
    // int one with eg. qOverload() as it's too new. So let's
    // use old style for signals
    //connect(ui->alphaSpinBox, &QSpinBox::valueChanged,
    //        this, &PreferencesDialog::alphaSpinBox_valueChanged);
    connect(ui->alphaSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(alphaSpinBox_valueChanged(int)));
    connect(ui->restoreDefaultsPushButton, &QPushButton::clicked,
            this, &PreferencesDialog::restoreDefaultsPushButton_clicked);

    ui->listWidget->setCurrentRow(0);

    m_colorMap[ui->hugeDiffPushButton] = &P->hugeDiffColor;
    m_colorMap[ui->identicalDiffPushButton] = &P->identicalDiffColor;
    m_colorMap[ui->moderateDiffPushButton] = &P->moderateDiffColor;
    m_colorMap[ui->notPresentDiffButton] = &P->notPresentDiffColor;
    m_colorMap[ui->keyWordPushButton] = &P->syntaxKeywordColor;
    m_colorMap[ui->jsonKeysPushButton] = &P->syntaxValueColor;

    QMap<QPushButton*, QColor*>::iterator i = m_colorMap.begin();
    while (i != m_colorMap.end()) {
        setupButton(i.key(), *i.value());
        connect(i.key(), &QPushButton::clicked,
                this, &PreferencesDialog::openColorDialog);
        ++i;
    }

    ui->alphaSpinBox->setValue(P->diffColorsAlpha);
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
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
        QString name(dia.selectedColor().name());
        c->setNamedColor(name);

        setupButton(b, *c);
    }
}

void PreferencesDialog::alphaSpinBox_valueChanged(int val)
{
    P->diffColorsAlpha = val;
}

void PreferencesDialog::restoreDefaultsPushButton_clicked()
{
    P->restoreDefaults();

    ui->alphaSpinBox->setValue(P->diffColorsAlpha);

    QMap<QPushButton*, QColor*>::iterator i = m_colorMap.begin();
    while (i != m_colorMap.end()) {
        setupButton(i.key(), *i.value());
        ++i;
    }
}
