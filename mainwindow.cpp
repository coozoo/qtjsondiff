/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "preferences/preferences.h"
#include "preferences/preferencesdialog.h"
#include "httprequestconfig.h"

// Parse a cURL command string and hand the config to the given
// container. Text can be inline (from --config) or file-loaded
// (from --config-file); the CLI parser has already resolved the
// source. When noHttpDefaults is false, any default header the
// parsed cURL DIDN'T set is appended - so users don't lose gzip
// decompression just because they wrote a short --config.
static void applyCliConfigToContainer(QJsonContainer *container,
                                      const QString &curlText,
                                      bool noHttpDefaults)
{
    if (!container || curlText.isEmpty()) return;
    bool ok = false;
    HttpRequestConfig cfg = HttpRequestConfig::fromCurlText(curlText, &ok);
    if (!ok)
        {
            qWarning() << "CLI config did not parse as a cURL command";
            return;
        }

    if (!noHttpDefaults)
        {
            const HttpRequestConfig d = HttpRequestConfig::defaults();
            for (const auto &defHdr : d.headers)
                {
                    bool userSetIt = false;
                    for (const auto &userHdr : cfg.headers)
                        {
                            if (userHdr.first.compare(defHdr.first,
                                                      Qt::CaseInsensitive) == 0)
                                {
                                    userSetIt = true;
                                    break;
                                }
                        }
                    if (!userSetIt)
                        cfg.headers.append(defHdr);
                }
        }

    container->setRequestConfig(cfg);
}


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
    messageJsonCont->setEditable(PREF_INST->editableSingleTree);
    //messageJsonCont->setBrowseVisible(false);
    messageJsonCont->loadJson(QString("{\"empty\":\"empty\"}"));
    differ = new QJsonDiff(ui->compare_tab);
    differ->setDiffEditable(PREF_INST->editableDiffView);
    // Live update: the Preferences dialog toggles checkboxes and emits
    // editModeChanged so we re-apply without restart.
    connect(PREF_INST, &Preferences::editModeChanged, this, [this]()
    {
        messageJsonCont->setEditable(PREF_INST->editableSingleTree);
        differ->setDiffEditable(PREF_INST->editableDiffView);
    });
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

    // loadLastPaths is NOT called here anymore. main.cpp calls it
    // BEFORE setDisplayMode: all three containers get their prev-
    // session paths restored, then setDisplayMode overrides only
    // the CLI-targeted container(s). So `qtjsondiff URL` shows
    // your URL on the single-tree tab, and switching to the diff
    // tab still shows what you had there last time.

    restoreGeometry(PREF_INST->mainWindowGeometry);
    restoreState(PREF_INST->mainWindowState);
}

void MainWindow::loadLastPathsIfEnabled()
{
    if (ui->openLast_action->isChecked())
        {
            loadLastPaths();
        }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setDisplayMode(const QStringList &files,
                                const QString &configCurl,
                                const QString &leftConfigCurl,
                                const QString &rightConfigCurl,
                                bool noHttpDefaults) {
    if (files.size() == 1) {
        ui->tabWidget->setCurrentWidget(ui->jsonview_tab);
        // Apply the config BEFORE loadJsonFile so if the target is a
        // URL, the very first request already carries the user's
        // custom method/headers/body.
        applyCliConfigToContainer(messageJsonCont, configCurl, noHttpDefaults);
        messageJsonCont->loadJsonFile(files.at(0));
    }
    else if (files.size() == 2) {
        ui->tabWidget->setCurrentWidget(ui->compare_tab);
        applyCliConfigToContainer(differ->left_cont,  leftConfigCurl,  noHttpDefaults);
        applyCliConfigToContainer(differ->right_cont, rightConfigCurl, noHttpDefaults);
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
    // Apply the persisted per-container cURL config BEFORE loadJsonFile
    // so a URL fetch on startup carries the user's method / headers /
    // body from the previous session - same ordering rule the CLI path
    // uses in setDisplayMode. noHttpDefaults=true here: what we saved
    // is exactly what the container last had (toCurlText), no need to
    // re-merge factory GET headers.
    applyCliConfigToContainer(differ->left_cont,
                              PREF_INST->differLeftCurl,  true);
    applyCliConfigToContainer(differ->right_cont,
                              PREF_INST->differRightCurl, true);
    applyCliConfigToContainer(messageJsonCont,
                              PREF_INST->jsonContainerCurl, true);

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

    // Snapshot each container's current HTTP request config as cURL
    // text. Default GETs serialize to a bare "curl -X GET" - fine to
    // save; loadLastPaths re-parses it back to defaults() shape.
    // Rides on save() below, gated by the same "Restore on start"
    // toggle at load time.
    if (messageJsonCont)
        PREF_INST->jsonContainerCurl =
            messageJsonCont->requestConfig().toCurlText();
    if (differ)
        {
            if (differ->left_cont)
                PREF_INST->differLeftCurl =
                    differ->left_cont->requestConfig().toCurlText();
            if (differ->right_cont)
                PREF_INST->differRightCurl =
                    differ->right_cont->requestConfig().toCurlText();
        }

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
