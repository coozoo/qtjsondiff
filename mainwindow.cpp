/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    openLast_toolbutton = new QToolButton();
    openLast_toolbutton->setText(tr("Restore on Start"));
    openLast_toolbutton->setToolTip(tr("Open last state on application start"));
    openLast_toolbutton->setCheckable(true);
    ui->mainToolBar->addWidget(openLast_toolbutton);
    openLast_toolbutton->setChecked(s.value(restore_on_start, QVariant()).toBool());

    connect(openLast_toolbutton, &QToolButton::clicked, this, &MainWindow::on_openLast_toolbutton_clicked);

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
    if (openLast_toolbutton->isChecked())
        {
            loadLastPaths();
        }

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::containerFileLoaded(QString path)
{
    QTextStream cout(stdout);
    cout << "Container got new file: " << path << endl;
    s.setValue(json_container_path, path);
}

void MainWindow::differLeftFileLoaded(QString path)
{
    QTextStream cout(stdout);
    cout << "Differ left container got new file: " << path << endl;
    s.setValue(differ_left_path, path);
}

void MainWindow::differRightFileLoaded(QString path)
{
    QTextStream cout(stdout);
    cout << "Differ right container got new file: " << path << endl;
    s.setValue(differ_right_path, path);
}

void MainWindow::loadLastPaths()
{
    differ->loadLeftJsonFile(s.value(differ_left_path, QVariant()).toString());
    differ->loadRightJsonFile(s.value(differ_right_path, QVariant()).toString());
    messageJsonCont->loadJsonFile(s.value(json_container_path, QVariant()).toString());
}

void MainWindow::on_openLast_toolbutton_clicked()
{
    s.setValue(restore_on_start, openLast_toolbutton->isChecked());
    if (openLast_toolbutton->isChecked())
        {
            loadLastPaths();
        }

}
