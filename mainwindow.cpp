/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "preferences/preferences.h"
#include "preferences/preferencesdialog.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->tabWidget->setCurrentIndex(PREF_INST->activeTabIndex);
    ui->tabWidget->setTabPosition(static_cast<QTabWidget::TabPosition>(PREF_INST->tabsPosition));
    //ui->tabWidget->setTabPosition(QTabWidget::TabPosition::East);
    ui->openLast_action->setChecked(PREF_INST->restoreOnStart);
    //hide statusbar I'm not using it :)
    ui->statusBar->hide();
    connect(ui->openLast_action, &QAction::toggled, this, &MainWindow::openLast_action_toggled);

    messageJsonCont = new QJsonContainer(ui->jsonview_tab);
    //messageJsonCont->setBrowseVisible(false);
    messageJsonCont->loadJson(QString("{\"empty\":\"empty\"}"));
    differ = new QJsonDiff(ui->compare_tab);
    QJsonDocument data22 = QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonLeft(data22);
    QJsonDocument data11 = QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonRight(data11);
    differ->expandIt();
    differ->setBrowseVisible(true);

    connect(messageJsonCont, &QJsonContainer::sJsonFileLoaded, this, &MainWindow::containerFileLoaded);
    connect(differ, &QJsonDiff::sRightJsonFileLoaded, this, &MainWindow::differRightFileLoaded);
    connect(differ, &QJsonDiff::sLeftJsonFileLoaded, this, &MainWindow::differLeftFileLoaded);
    connect(ui->actionPreferences, &QAction::triggered,
            this, &MainWindow::actionPreferences_triggered);

    if (ui->openLast_action->isChecked())
        {
            loadLastPaths();
        }

    restoreGeometry(PREF_INST->mainWindowGeometry);
    restoreState(PREF_INST->mainWindowState);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setDisplayMode(const QStringList &files) {
    if (files.size() == 1) {
        ui->tabWidget->setCurrentWidget(ui->jsonview_tab);
        messageJsonCont->loadJsonFile(files.at(0));
    }
    else if (files.size() == 2) {
        ui->tabWidget->setCurrentWidget(ui->compare_tab);
        differ->loadLeftJsonFile(files.at(0));
        differ->loadRightJsonFile(files.at(1));
    }
}

void MainWindow::containerFileLoaded(const QString &path)
{
    qDebug() << "Container got new file: " << path;
    PREF_INST->jsonContainerPath = path;
}

void MainWindow::differLeftFileLoaded(const QString &path)
{
    qDebug() << "Differ left container got new file: " << path;
    PREF_INST->differLeftPath = path;
}

void MainWindow::differRightFileLoaded(const QString &path)
{
    qDebug() << "Differ right container got new file: " << path;
    PREF_INST->differRightPath = path;
}

void MainWindow::loadLastPaths()
{
    differ->loadLeftJsonFile(PREF_INST->differLeftPath);
    differ->loadRightJsonFile(PREF_INST->differRightPath);
    messageJsonCont->loadJsonFile(PREF_INST->jsonContainerPath);
}

void MainWindow::openLast_action_toggled(bool state)
{
    PREF_INST->restoreOnStart = state;
    if (state)
        {
            loadLastPaths();
        }

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    PREF_INST->activeTabIndex = ui->tabWidget->currentIndex();
    PREF_INST->mainWindowGeometry = saveGeometry();
    PREF_INST->mainWindowState = saveState();

    PREF_INST->save();

    QMainWindow::closeEvent(event);
}

void MainWindow::actionPreferences_triggered()
{
    PreferencesDialog d;
    d.exec();
    ui->tabWidget->setTabPosition(static_cast<QTabWidget::TabPosition>(PREF_INST->tabsPosition));
    messageJsonCont->showJsonButtonPosition();
    differ->showJsonButtonPosition();
}
