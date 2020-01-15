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
    //messageJsonCont->loadJson(QString("{\"empty\":\"empty\"}"));
    messageJsonCont->loadJson(QString("[{\"lot\": \"mean\",\"began\": [-72347258,[\"quarter\",\"already\",2099354034.981699,\"tape\",1019017406],false,{\"torn\": {\"baby\": 2074636392,\"doing\": -1881011261.3109884,\"hurried\": -731482241.8732705,\"girl\": \"effort\"},\"chart\": \"desk\",\"sang\": \"find\",\"failed\": 308551425.02163744,\"slipped\": -1576032008.5307245},-80775944],\"serve\": false,\"thought\": -1187891361,\"blind\":\"truth\"},true,false,false,939501492]"));
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

