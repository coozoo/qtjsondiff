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

    // Optional CLI configs. Each is the cURL text (inline or
    // file-loaded - the CLI parser has already resolved the source)
    // to apply to the matching container before its URL is loaded.
    // Empty strings are ignored.
    void setDisplayMode(const QStringList &files,
                        const QString &configCurl = QString(),
                        const QString &leftConfigCurl = QString(),
                        const QString &rightConfigCurl = QString(),
                        bool noHttpDefaults = false);

    // Public wrapper for main() to trigger the "restore-last-loaded
    // paths on startup" behavior. Not called from the ctor anymore
    // - main.cpp only invokes it when the CLI provided no
    // positionals, so `qtjsondiff URL` doesn't also load the
    // previous session's diff URLs into the diff tab.
    void loadLastPathsIfEnabled();

public slots:
    void containerFileLoaded(const QString &path);
    void differLeftFileLoaded(const QString &path);
    void differRightFileLoaded(const QString &path);

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
