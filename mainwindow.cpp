/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(s.value(active_tab_index, QVariant()).toInt()?s.value(active_tab_index, QVariant()).toInt():0);

    ui->openLast_action->setChecked(s.value(restore_on_start, QVariant()).toBool());

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
    if (ui->openLast_action->isChecked())
        {
            loadLastPaths();
        }

    restoreGeometry(s.value("MainWindow/geometry").toByteArray());
    restoreState(s.value("MainWindow/windowState").toByteArray());
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
    s.setValue(json_container_path, path);
}

void MainWindow::differLeftFileLoaded(QString path)
{
    qDebug() << "Differ left container got new file: " << path;
    s.setValue(differ_left_path, path);
}

void MainWindow::differRightFileLoaded(QString path)
{
    qDebug() << "Differ right container got new file: " << path;
    s.setValue(differ_right_path, path);
}

void MainWindow::loadLastPaths()
{
    differ->loadLeftJsonFile(s.value(differ_left_path, QVariant()).toString());
    differ->loadRightJsonFile(s.value(differ_right_path, QVariant()).toString());
    messageJsonCont->loadJsonFile(s.value(json_container_path, QVariant()).toString());
}

void MainWindow::openLast_action_toggled(bool state)
{
    s.setValue(restore_on_start, state);
    if (state)
        {
            loadLastPaths();
        }

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    s.setValue(active_tab_index,ui->tabWidget->currentIndex());
    s.setValue("MainWindow/geometry", saveGeometry());
    s.setValue("MainWindow/windowState", saveState());

    QMainWindow::closeEvent(event);
}
