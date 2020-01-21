/* Author: Yuriy Kuzin
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include "qjsonitem.h"
#include "qjsonmodel.h"
#include "qjsoncontainer.h"
#include "qjsondiff.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QJsonContainer *messageJsonCont;
    QJsonDiff *differ;

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
