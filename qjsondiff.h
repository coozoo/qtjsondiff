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

#include "qjsoncontainer.h"
#include"qjsonitem.h"
#include"qjsonmodel.h"

class QPaintEvent;

class QJsonDiff : public QWidget
{
    Q_OBJECT

public:
    explicit QJsonDiff(QWidget *parent = nullptr);
    ~QJsonDiff() override;
    QJsonContainer *left_cont;
    QJsonContainer *right_cont;
    void loadJsonLeft(QJsonDocument data);
    void loadJsonLeft(QString data);
    void loadJsonRight(QJsonDocument data);
    void loadJsonRight(QString data);
    QTreeView* getLeftTreeView();
    QTreeView* getRightTreeView();
    QJsonModel* getLeftJsonModel();
    QJsonModel* getRightJsonModel();
    QGridLayout *qjsoncontainer_layout;
    QGroupBox *container_left_groupbox;
    QGroupBox *container_right_groupbox;
    QGroupBox * common_groupbox;
    QVBoxLayout *common_layout;
    QPushButton *compare_pushbutton;
    void expandIt();
    QCheckBox *syncScroll_checkbox;
    QCheckBox *useFullPath_checkbox;
    QSpacerItem *checkboxSpacer;

    void compareModels(QJsonModel *modelLeft, const QModelIndex &parentLeft, QJsonModel *modelRight);
    int findIndexInModel(QJsonModel *modelLeft, QJsonTreeItem *itemLeft, QModelIndex idxLeft, QJsonModel *modelRight, const QModelIndex &parentRight);
    int fixColors(QJsonModel *model, const QModelIndex &parent);
    QStringList jsonPathList(QJsonModel * model, const QModelIndex &parent, QList<QModelIndex> *indexList);
    void comparePath(QJsonModel *modelLeft, QStringList leftPathList, QList<QModelIndex> leftIndexList,
                                     QJsonModel *modelRight,QStringList rightPathList, QList<QModelIndex> rightIndexList);
    void compareValue(QJsonModel *modelLeft, QList<QModelIndex> leftIndexList,
                                     QJsonModel *modelRight);
    int prevLeftScroll;
    int prevRightScroll;
    void startComparison();
    void setBrowseVisible(bool state);

    const QColor identicalDiffColor=QColor(0, 100, 0, 150);
    const QColor moderateDiffColor=QColor(Qt::yellow);
    const QColor hugeDiffColor=QColor(Qt::red);
    const QColor notPresentDiffColor=QColor(Qt::lightGray);

signals:
    void sLoadRightJsonFile(QString target);
    void sLoadLeftJsonFile(QString target);
    void sRightJsonFileLoaded(QString path);
    void sLeftJsonFileLoaded(QString path);

public slots:
    void on_compare_pushbutton_clicked();
    void on_lefttreeview_clicked(const QModelIndex&);
    void on_righttreeview_clicked(const QModelIndex&);
    void on_lefttreeview_scroll_valuechanged(int);
    void on_righttreeview_scroll_valuechanged(int);
    void on_useFullPath_checkbox_stateChanged(int);
    void reinitRightModel();
    void reinitLeftModel();
    void loadRightJsonFile(QString target);
    void loadLeftJsonFile(QString target);
    void rightJsonFileLoaded(QString path);
    void leftJsonFileLoaded(QString path);

protected:
    void paintEvent(QPaintEvent *) override;
};

#endif // QJSONDIFF_H
