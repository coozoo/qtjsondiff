#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QMap>

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

private:
    Ui::PreferencesDialog *ui;

    void setupButton(QPushButton *b, const QColor &c);

    QMap<QPushButton*, QColor*> m_colorMap;

private slots:
    void openColorDialog();
    void alphaSpinBox_valueChanged(int val);
    void restoreDefaultsPushButton_clicked();
    void on_tabpos_button_clicked(int id);
    void on_showJsonButtonPosition_clicked(int id);
};

#endif // PREFERENCESDIALOG_H
