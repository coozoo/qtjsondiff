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
    QToolButton *openLast_toolbutton;
    QSettings s;

public slots:
    void containerFileLoaded(QString path);
    void differLeftFileLoaded(QString path);
    void differRightFileLoaded(QString path);

    void on_openLast_toolbutton_clicked();

    void saveSettings();

private:
    Ui::MainWindow *ui;

    void loadLastPaths();

    const QString json_container_path="Saved_Paths/json_container_path";
    const QString differ_left_path="Saved_Paths/differ_left_path";
    const QString differ_right_path="Saved_Paths/differ_right_path";
    const QString restore_on_start="Saved_Paths/restore_on_start";
    const QString active_tab_index="active_tab_index";

};

#endif // MAINWINDOW_H
