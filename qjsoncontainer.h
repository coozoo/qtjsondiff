/* Author: Yuriy Kuzin
 */
#ifndef QJSONCONTAINER_H
#define QJSONCONTAINER_H

#include <QObject>
#include <QWidget>
#include <qjsonmodel.h>
#include <QTreeView>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolTip>
#include <QToolBar>
#include <QMenu>
#include <QClipboard>
#include <QApplication>

#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSsl>
#include <zlib.h>

class QJsonContainer : public QWidget
{
    Q_OBJECT
public:
    explicit QJsonContainer(QWidget *parent=nullptr);
    ~QJsonContainer();

    QJsonModel *model;
    QTreeView *treeview;
    QPlainTextEdit *viewjson_plaintext;
    QCheckBox *expandAll_Checkbox;
    void loadJson(QJsonDocument data);
    void loadJson(QString data);
    QString getJson(QList<QModelIndex> jsonPath);
    QTreeView* getTreeView();
    QJsonModel* getJsonModel();
    QVBoxLayout *treeview_layout;
    QGroupBox *treeview_groupbox;
    QVBoxLayout *obj_layout;
    void expandIt();
    QGroupBox *browse_groupBox;
    QLineEdit *filePath_lineEdit;
    QLineEdit *find_lineEdit;
    QToolButton* browse_toolButton;
    QToolButton* sortObj_toolButton;
    QGridLayout* tools_layout;
    QToolBar *toolbar;
    QWidget *spacer;
    QHBoxLayout *browse_layout;
    void setBrowseVisible(bool state);
    QPushButton *showjson_pushbutton;
    QToolButton *findNext_toolbutton;
    QToolButton *findPrevious_toolbutton;
    QToolButton *findCaseSensitivity_toolbutton;
    void reInitModel();
    QStringList extractStringsFromModel(QJsonModel *model, const QModelIndex &parent);
    QStringList extractItemTextFromModel(const QModelIndex &parent);
    QList<QModelIndex> findModelText(QJsonModel *model, const QModelIndex &parent);
    int currentIndexFinder(QJsonModel *model, const QModelIndex &parent, QList<QModelIndex> *currentFindIndexesList, QModelIndex selectedIndex, bool &matchedSelectedIndex, int &indexid);
    //QJsonDocument sortObjectArrays(QJsonDocument data);
    //QJsonArray sortObjectArraysGrabArray(QJsonArray data);
    //QJsonObject sortObjectArraysGrabObject(QJsonObject data);
    static int countStringWeight(QString inStr);
    static bool wayToSort(const QJsonValue &v1, const QJsonValue &v2);
    void getData();

private:
    QByteArray gUncompress(const QByteArray &data);
    //variables to handle serach nodes in tree
    QList<QModelIndex> currentFindIndexesList;
    QString currentFindText;
    int currentFindIndexId;
    void resetCurrentFind();
    void findTextJsonIndexHandler(bool direction);
    QMenu myMenu;
    QAction *copyRow;
    QAction *copyRows;
    QAction *copyPath;
    QAction *copyJsonPlainText;
    QAction *copyJsonPrettyText;
    QAction *copyJsonByPath;

signals:
    void sOpenJsonFile();
    void jsonUpdated();


private slots:
    void on_expandAll_checkbox_marked();
    void on_treeview_item_expanded();
    void on_browse_toolButton_clicked();
    void on_showjson_pushbutton_clicked();
    //void on_sortObj_toolButton_clicked();
    void on_findNext_toolbutton_clicked();
    void on_findPrevious_toolbutton_clicked();
    void on_findCaseSensitivity_toolbutton_clicked();
    void openJsonFile();
    void serviceGetDataRequestFinished(QNetworkReply* reply);
    void on_find_lineEdit_textChanged(QString text);
    void on_model_dataUpdated();
    void showContextMenu(const QPoint &point);
public slots:
    void findText();

protected:
    bool eventFilter(QObject* obj, QEvent *event);

};



#endif // QJSONCONTAINER_H
