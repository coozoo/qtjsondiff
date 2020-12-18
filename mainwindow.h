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
    ~MainWindow() override;
    QJsonContainer *messageJsonCont;
    QJsonDiff *differ;

    void setDisplayMode(const QStringList &files);

public slots:
    void containerFileLoaded(QString path);
    void differLeftFileLoaded(QString path);
    void differRightFileLoaded(QString path);

    void openLast_action_toggled(bool state);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;

    void loadLastPaths();

private slots:
    void actionPreferences_triggered();
};

#endif // MAINWINDOW_H
