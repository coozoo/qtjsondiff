#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QMap>
#include <QAbstractButton>
#include <QStandardItemModel>

namespace Ui {
class PreferencesDialog;
}

/**
 * @brief The PreferencesDialog class
 *
 * Prefs are applied to the singleton automatically and instantly.
 */
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::PreferencesDialog *ui;

    void setupButton(QPushButton *b, const QColor &c);
    void populateShortcutsTable();

    QMap<QPushButton*, QColor*> m_colorMap;

private slots:
    void openColorDialog();
    void alphaSpinBox_valueChanged(int val);
    void restoreColorDefaultsPushButton_clicked();
    void restoreShortcutDefaultsPushButton_clicked();
    void tabsPosition_button_clicked(QAbstractButton* button);
    void showJsonButtonPosition_clicked(QAbstractButton* button);
    void shortcut_changed(QStandardItem *item);
    void editModeCheckBoxToggled(bool checked);
    void appStyleComboBox_currentIndexChanged(int index);
    void useStyledTreeCheckBox_toggled(bool checked);
};

#endif // PREFERENCESDIALOG_H
