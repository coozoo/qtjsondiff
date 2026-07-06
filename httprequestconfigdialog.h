/* Author: Yuriy Kuzin
 */
#ifndef HTTPREQUESTCONFIGDIALOG_H
#define HTTPREQUESTCONFIGDIALOG_H

#include <QDialog>
#include "httprequestconfig.h"

class HttpRequestConfigWidget;

// Thin QDialog wrapper around HttpRequestConfigWidget. The widget
// carries its own Save/Load/Reset controls (so it stays reusable
// standalone); the dialog only adds OK/Cancel semantics so a caller
// can prompt-and-commit.
class HttpRequestConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HttpRequestConfigDialog(QWidget *parent = nullptr);

    HttpRequestConfig config() const;
    void setConfig(const HttpRequestConfig &config);

private:
    HttpRequestConfigWidget *mWidget;
};

#endif // HTTPREQUESTCONFIGDIALOG_H
