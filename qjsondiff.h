/* Author: Yuriy Kuzin
 */
#ifndef QJSONDIFF_H
#define QJSONDIFF_H

#include <QObject>
#include <QWidget>
#include <QHBoxLayout>
#include <QMargins>
#include <QPushButton>
#include <QModelIndex>
#include <QScrollBar>
#include <QSpacerItem>
#include <QShortcut>

#include "qjsoncontainer.h"
#include"qjsonitem.h"
#include"qjsonmodel.h"
#include "jsondiffengine.h"

class QPaintEvent;
class QThread;
class QProgressDialog;

class QJsonDiff : public QWidget
{
    Q_OBJECT

public:
    explicit QJsonDiff(QWidget *parent = nullptr);
    ~QJsonDiff() override;
    QJsonContainer *left_cont;
    QJsonContainer *right_cont;
    void loadJsonLeft(QJsonDocument data);
    void loadJsonLeft(const QString& data);
    void loadJsonRight(QJsonDocument data);
    void loadJsonRight(const QString &data);
    QTreeView* getLeftTreeView();
    QTreeView* getRightTreeView();
    QJsonModel* getLeftJsonModel();
    QJsonModel* getRightJsonModel();
    QGridLayout *qjsoncontainer_layout;
    QGroupBox *container_left_groupbox;
    QGroupBox *container_right_groupbox;
    QGroupBox * common_groupbox;
    QVBoxLayout *common_layout;
    QGroupBox *button_groupbox;
    QGridLayout *button_layout;
    QGroupBox *compare_groupbox;
    QGridLayout *compare_layout;
    QPushButton *compare_pushbutton;
    QShortcut *compare_shortcut;
    void expandIt();
    QCheckBox *syncScroll_checkbox;
    QCheckBox *useFullPath_checkbox;
    QSpacerItem *checkboxSpacer;

    int prevLeftScroll;
    int prevRightScroll;
    // Synchronous compare on the calling thread. Used by integrators
    // and tests. The Compare button uses the threaded path instead
    // (see on_compare_pushbutton_clicked).
    void startComparison();
    void setBrowseVisible(bool state);
    void showJsonButtonPosition();

signals:
    void sLoadRightJsonFile(const QString &target);
    void sLoadLeftJsonFile(const QString &target);
    void sRightJsonFileLoaded(const QString &path);
    void sLeftJsonFileLoaded(const QString &path);

public slots:
    void on_compare_pushbutton_clicked();
    void on_lefttreeview_clicked(const QModelIndex&);
    void on_righttreeview_clicked(const QModelIndex&);
    void on_lefttreeview_scroll_valuechanged(int);
    void on_righttreeview_scroll_valuechanged(int);
    void on_useFullPath_checkbox_stateChanged(int);
    void reinitRightModel();
    void reinitLeftModel();
    void loadRightJsonFile(const QString &target);
    void loadLeftJsonFile(const QString &target);
    void rightJsonFileLoaded(const QString &path);
    void leftJsonFileLoaded(const QString &path);

private slots:
    void onCompareFinished(QSharedPointer<DiffNode> left, QSharedPointer<DiffNode> right);
    void onCompareCancelled();
    void onCompareProgressed(int done, int total);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    // Worker thread + engine used by the Compare button's async path.
    // startComparison() (the public API) stays synchronous and does
    // not touch these.
    JsonDiffEngine  *mEngine = nullptr;
    QThread         *mWorkerThread = nullptr;
    QProgressDialog *mProgressDialog = nullptr;
    bool             mCompareCancelledByUser = false;

    void setupCompareWorker();
    void teardownCompareWorker();
};

#endif // QJSONDIFF_H
