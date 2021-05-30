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
#include <QLabel>
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

#include <QWidgetAction>

typedef QList<QPair<QModelIndex, QColor> > QLinHeaderList;

class JsonSyntaxHighlighter;

QT_BEGIN_NAMESPACE
class QMimeData;
QT_END_NAMESPACE

inline void swap(QJsonValueRef v1, QJsonValueRef v2)
{
    QJsonValue temp(v1);
    v1 = QJsonValue(v2);
    v2 = temp;
}

class QJsonContainer : public QWidget
{
    Q_OBJECT

public:
    explicit QJsonContainer(QWidget *parent=nullptr);
    ~QJsonContainer() override;

    QJsonModel *model;
    QTreeView *treeview;
    QPlainTextEdit *viewjson_plaintext;
    QAction *expandAll_Checkbox;
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
    QToolButton* refresh_toolButton;
    QAction *sortObj_toolButton;
    QAction *switchview_action;
    QGridLayout* tools_layout;
    QToolBar *toolbar;
    QWidget *spacer;
    QHBoxLayout *browse_layout;
    void setBrowseVisible(bool state);
    QPushButton *showjson_pushbutton;
    QAction *findNext_toolbutton;
    QAction *findPrevious_toolbutton;
    QAction *findCaseSensitivity_toolbutton;
    QAction *goToNextDiff_toolbutton=nullptr;
    QAction *goToPreviousDiff_toolbutton=nullptr;
    QLineEdit *diffAmount_lineEdit=nullptr;
    QList<QModelIndex> fillGotoList(QJsonModel *model, const QModelIndex &parent);
    void reInitModel();
    QStringList extractStringsFromModel(QJsonModel *model, const QModelIndex &parent);
    QStringList extractItemTextFromModel(const QModelIndex &parent);
    QList<QModelIndex> findModelText(QJsonModel *model, const QModelIndex &parent);
    int currentIndexFinder(QJsonModel *model, const QModelIndex &parent, QList<QModelIndex> *currentFindIndexesList, QModelIndex selectedIndex, bool &matchedSelectedIndex, int &indexid);
    QJsonDocument sortObjectArrays(QJsonDocument data);
    QJsonArray sortObjectArraysGrabArray(QJsonArray data);
    QJsonObject sortObjectArraysGrabObject(QJsonObject data);
    static int countStringWeight(QString inStr);
    static bool wayToSort(const QJsonValue &v1, const QJsonValue &v2);
    void getData();
    void showGoto(bool show);
    void diffAmountUpdate();
    void gotoIndexHandler(bool directionForward);

private:
    JsonSyntaxHighlighter *viewJsonSyntaxHighlighter;
    QByteArray gUncompress(const QByteArray &data);
    //variables to handle serach nodes in tree
    QList<QModelIndex> currentFindIndexesList;
    QString currentFindText;
    int currentFindIndexId;
    void resetCurrentFind();
    void findTextJsonIndexHandler(bool direction);
    int currentGotoIndexId;
    QMenu myMenu;
    QAction *copyRow;
    QAction *copyRows;
    QAction *copyPath;
    QAction *copyJsonPlainText;
    QAction *copyJsonPrettyText;
    QAction *copyJsonByPath;
    QAction *singleExpandSelectedRecursively;
    QAction *singleCollapseSelectedRecursively;
    QMenu multiSelectMenu;
    QAction *expandSelected;
    QAction *collapseSelected;
    QAction *expandSelectedRecursively;
    QAction *collapseSelectedRecursively;
    void expandRecursively(const QModelIndex &index, QTreeView *view,bool expand);
    QAction* toolbarbutton=nullptr;
    QList<QModelIndex> gotoIndexes_list;
    void resetGoto();
    QPalette gotDefaultPalette;
    QPixmap createPixmapFromText(const QString &text);

signals:
    void sJsonFileLoaded(QString path);
    void jsonUpdated();
    void diffSelected(QModelIndex &index);

private slots:
    void on_expandAll_checkbox_marked();
    void on_treeview_item_expanded();
    void on_browse_toolButton_clicked();
    void on_refresh_toolButton_clicked();
    void on_showjson_pushbutton_clicked();
    void on_sortObj_toolButton_clicked();
    void on_findNext_toolbutton_clicked();
    void on_findPrevious_toolbutton_clicked();
    void on_findCaseSensitivity_toolbutton_clicked();
    void on_GoToNextDiff_toolbutton_clicked();
    void on_GoToPreviousDiff_toolbutton_clicked();
    void openJsonFile();
    void serviceGetDataRequestFinished(QNetworkReply* reply);
    void on_find_lineEdit_textChanged(const QString &text);
    void on_model_dataUpdated();
    void showContextMenu(const QPoint &point);

public slots:
    void findText();
    void loadJsonFile(QString target);
    void showJsonButtonPosition();

protected:
    bool eventFilter(QObject* obj, QEvent *event) override;

};





#endif // QJSONCONTAINER_H
