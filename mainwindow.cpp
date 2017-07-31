/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    messageJsonCont=new QJsonContainer(ui->jsonview_tab);
    //messageJsonCont->setBrowseVisible(false);
    messageJsonCont->sortObj_toolButton->setVisible(true);
    messageJsonCont->loadJson(QString("{\"empty\":\"empty\"}"));
    differ=new QJsonDiff(ui->compare_tab);
    QJsonDocument data22=QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonLeft(data22);
    QJsonDocument data11=QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonRight(data11);
    differ->expandIt();
    differ->setBrowseVisible(true);

}

MainWindow::~MainWindow()
{
    delete ui;
}

