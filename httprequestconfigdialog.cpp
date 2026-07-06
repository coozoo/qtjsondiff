/* Author: Yuriy Kuzin
 */
#include "httprequestconfigdialog.h"
#include "httprequestconfigwidget.h"

#include <QVBoxLayout>
#include <QDialogButtonBox>

HttpRequestConfigDialog::HttpRequestConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Configure HTTP request"));
    resize(640, 560);

    auto *layout = new QVBoxLayout(this);
    mWidget = new HttpRequestConfigWidget(this);
    layout->addWidget(mWidget, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

HttpRequestConfig HttpRequestConfigDialog::config() const
{
    return mWidget->config();
}

void HttpRequestConfigDialog::setConfig(const HttpRequestConfig &config)
{
    mWidget->setConfig(config);
}
