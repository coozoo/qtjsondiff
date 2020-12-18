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


    ui->tabWidget->setCurrentIndex(P->activeTabIndex);
    ui->openLast_action->setChecked(P->restoreOnStart);

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

    restoreGeometry(P->mainWindowGeometry);
    restoreState(P->mainWindowState);
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

void MainWindow::containerFileLoaded(QString path)
{
    qDebug() << "Container got new file: " << path;
    P->jsonContainerPath = path;
}

void MainWindow::differLeftFileLoaded(QString path)
{
    qDebug() << "Differ left container got new file: " << path;
    P->differLeftPath = path;
}

void MainWindow::differRightFileLoaded(QString path)
{
    qDebug() << "Differ right container got new file: " << path;
    P->differRightPath = path;
}

void MainWindow::loadLastPaths()
{
    differ->loadLeftJsonFile(P->differLeftPath);
    differ->loadRightJsonFile(P->differRightPath);
    messageJsonCont->loadJsonFile(P->jsonContainerPath);
}

void MainWindow::openLast_action_toggled(bool state)
{
    P->restoreOnStart = state;
    if (state)
        {
            loadLastPaths();
        }

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    P->activeTabIndex = ui->tabWidget->currentIndex();
    P->mainWindowGeometry = saveGeometry();
    P->mainWindowState = saveState();

    P->save();

    QMainWindow::closeEvent(event);
}

void MainWindow::actionPreferences_triggered()
{
    PreferencesDialog d;
    d.exec();
}
