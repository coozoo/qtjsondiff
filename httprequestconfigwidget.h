/* Author: Yuriy Kuzin
 */
#ifndef HTTPREQUESTCONFIGWIDGET_H
#define HTTPREQUESTCONFIGWIDGET_H

#include <QWidget>
#include <QList>
#include "httprequestconfig.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QVBoxLayout;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QLineEdit;
class QTabWidget;
class QLabel;
QT_END_NAMESPACE

// Reusable request-configuration editor. Two tabs - a form view
// (method combo, dynamic header rows, body edit) and a raw cURL view
// that round-trips through HttpRequestConfig::toCurlText /
// fromCurlText. Save…/Load… buttons persist a config to a plain text
// file on disk (also cURL syntax) so presets are just files the user
// manages themselves - no QSettings involvement.
class HttpRequestConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HttpRequestConfigWidget(QWidget *parent = nullptr);

    HttpRequestConfig config() const;
    void setConfig(const HttpRequestConfig &config);

    // Header names commonly seen on real requests. Fed into the
    // QCompleter on each header-key field. Exposed static so
    // integrators embedding the widget can extend it if they want.
    static QStringList commonHeaderNames();

signals:
    void configChanged();

public slots:
    void resetToDefaults();
    void saveToFile();
    void loadFromFile();

private slots:
    void onAddHeaderClicked();
    void onRemoveHeaderRowClicked();
    void onMethodChanged(int index);
    void onTabChanged(int index);

private:
    struct HeaderRow
    {
        QWidget *rowWidget;
        QLineEdit *keyEdit;
        QLineEdit *valueEdit;
        QToolButton *removeButton;
    };

    void buildUi();
    HeaderRow addHeaderRow(const QString &key = QString(),
                           const QString &value = QString());
    void clearHeaderRows();
    // Read the form widgets and build a config (used by config()
    // and to refresh the cURL tab).
    HttpRequestConfig readFormConfig() const;
    // Populate the form widgets from a config (used by setConfig
    // and when switching from cURL tab to form tab).
    void populateForm(const HttpRequestConfig &config);
    // Body edit is only meaningful for methods that carry a body.
    void updateBodyEnabledState();

    QTabWidget *mTabs = nullptr;
    int mFormTabIndex = 0;
    int mCurlTabIndex = 1;

    // Form tab
    QComboBox *mMethodCombo = nullptr;
    QWidget *mHeadersHost = nullptr;
    QVBoxLayout *mHeadersLayout = nullptr;
    QPushButton *mAddHeaderButton = nullptr;
    QPlainTextEdit *mBodyEdit = nullptr;
    QList<HeaderRow> mHeaderRows;

    // cURL tab
    QPlainTextEdit *mCurlEdit = nullptr;
    QLabel *mCurlParseWarning = nullptr;

    // URL is not editable on the form tab (it belongs to the
    // container's own filePath_lineEdit), but the cURL tab both shows
    // and parses it. Remember whatever URL came in via setConfig / a
    // cURL-tab edit so switching tabs Form -> cURL -> Form -> cURL
    // doesn't silently wipe it out of readFormConfig().
    QString mFormUrl;

    // Shared buttons
    QPushButton *mResetButton = nullptr;
    QPushButton *mSaveButton = nullptr;
    QPushButton *mLoadButton = nullptr;
};

#endif // HTTPREQUESTCONFIGWIDGET_H
