/* Author: Yuriy Kuzin
 */
#include "httprequestconfigwidget.h"

#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QTabWidget>
#include <QLabel>
#include <QCompleter>
#include <QStringListModel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QScrollArea>
#include <QGroupBox>

QStringList HttpRequestConfigWidget::commonHeaderNames()
{
    // A pragmatic subset of the RFC 7231 / 9110 fields plus the
    // custom-header names that show up on most API calls.
    return QStringList{
        QStringLiteral("Accept"),
        QStringLiteral("Accept-Charset"),
        QStringLiteral("Accept-Encoding"),
        QStringLiteral("Accept-Language"),
        QStringLiteral("Authorization"),
        QStringLiteral("Cache-Control"),
        QStringLiteral("Connection"),
        QStringLiteral("Content-Encoding"),
        QStringLiteral("Content-Language"),
        QStringLiteral("Content-Length"),
        QStringLiteral("Content-Type"),
        QStringLiteral("Cookie"),
        QStringLiteral("Date"),
        QStringLiteral("DNT"),
        QStringLiteral("ETag"),
        QStringLiteral("Expect"),
        QStringLiteral("Forwarded"),
        QStringLiteral("From"),
        QStringLiteral("Host"),
        QStringLiteral("If-Match"),
        QStringLiteral("If-Modified-Since"),
        QStringLiteral("If-None-Match"),
        QStringLiteral("If-Range"),
        QStringLiteral("If-Unmodified-Since"),
        QStringLiteral("Origin"),
        QStringLiteral("Pragma"),
        QStringLiteral("Range"),
        QStringLiteral("Referer"),
        QStringLiteral("User-Agent"),
        QStringLiteral("X-Api-Key"),
        QStringLiteral("X-Auth-Token"),
        QStringLiteral("X-CSRF-Token"),
        QStringLiteral("X-Forwarded-For"),
        QStringLiteral("X-Requested-With"),
    };
}

HttpRequestConfigWidget::HttpRequestConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    populateForm(HttpRequestConfig::defaults());
}

void HttpRequestConfigWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    mTabs = new QTabWidget(this);

    // ---- Form tab ----
    auto *formPage = new QWidget(mTabs);
    auto *formLayout = new QVBoxLayout(formPage);
    formLayout->setSpacing(8);

    // Method row
    auto *methodRow = new QHBoxLayout;
    methodRow->addWidget(new QLabel(tr("Method:"), formPage));
    mMethodCombo = new QComboBox(formPage);
    mMethodCombo->addItem(QStringLiteral("GET"),    HttpRequestConfig::Get);
    mMethodCombo->addItem(QStringLiteral("POST"),   HttpRequestConfig::Post);
    mMethodCombo->addItem(QStringLiteral("PUT"),    HttpRequestConfig::Put);
    mMethodCombo->addItem(QStringLiteral("PATCH"),  HttpRequestConfig::Patch);
    mMethodCombo->addItem(QStringLiteral("DELETE"), HttpRequestConfig::Delete);
    mMethodCombo->addItem(QStringLiteral("HEAD"),   HttpRequestConfig::Head);
    methodRow->addWidget(mMethodCombo);
    methodRow->addStretch(1);
    formLayout->addLayout(methodRow);

    // ---- Headers section ----
    // Skipping QGroupBox on purpose - some styles let child content
    // drift over the title label. A plain "Headers" label + a "+ Add"
    // button on the same row, followed by rows in a scroll area, is
    // both cheaper and behaves predictably.
    auto *headersTitleRow = new QHBoxLayout;
    auto *headersLabel = new QLabel(tr("Headers"), formPage);
    QFont headersFont = headersLabel->font();
    headersFont.setBold(true);
    headersLabel->setFont(headersFont);
    mAddHeaderButton = new QPushButton(tr("+ Add header"), formPage);
    headersTitleRow->addWidget(headersLabel);
    headersTitleRow->addStretch(1);
    headersTitleRow->addWidget(mAddHeaderButton);
    formLayout->addLayout(headersTitleRow);

    auto *headersScroll = new QScrollArea(formPage);
    headersScroll->setWidgetResizable(true);
    headersScroll->setFrameShape(QFrame::StyledPanel);
    // Guard against the "narrow dialog squeezes 'Accept-Encoding' /
    // 'gzip, deflate' unreadable" case: give the whole scroll area a
    // reasonable minimum width so the key/value QLineEdits get room
    // to lay out.
    headersScroll->setMinimumWidth(520);
    headersScroll->setMinimumHeight(140);

    mHeadersHost = new QWidget;
    mHeadersLayout = new QVBoxLayout(mHeadersHost);
    mHeadersLayout->setContentsMargins(4, 4, 4, 4);
    mHeadersLayout->setSpacing(4);
    // Bottom stretch keeps rows anchored to the top when there are
    // few of them. addHeaderRow inserts BEFORE this stretch so new
    // rows always land at the bottom of the visible list.
    mHeadersLayout->addStretch(1);
    headersScroll->setWidget(mHeadersHost);
    formLayout->addWidget(headersScroll, 1);

    // ---- Body section ----
    auto *bodyLabel = new QLabel(tr("Body"), formPage);
    bodyLabel->setFont(headersFont);
    formLayout->addWidget(bodyLabel);

    mBodyEdit = new QPlainTextEdit(formPage);
    mBodyEdit->setPlaceholderText(
        tr("Request body - used for POST/PUT/PATCH/DELETE."));
    mBodyEdit->setMinimumHeight(80);
    formLayout->addWidget(mBodyEdit, 1);

    mFormTabIndex = mTabs->addTab(formPage, tr("Form"));

    // ---- cURL tab ----
    auto *curlPage = new QWidget(mTabs);
    auto *curlLayout = new QVBoxLayout(curlPage);
    auto *curlHelp = new QLabel(
        tr("Paste a cURL command (e.g. from browser devtools) or edit "
           "the one below. Switch tabs to apply."),
        curlPage);
    curlHelp->setWordWrap(true);
    curlLayout->addWidget(curlHelp);
    mCurlEdit = new QPlainTextEdit(curlPage);
    mCurlEdit->setPlaceholderText(
        QStringLiteral("curl -X POST 'https://…' \\\n"
                       "  -H 'Content-Type: application/json' \\\n"
                       "  --data-raw '{…}'"));
    curlLayout->addWidget(mCurlEdit, 1);
    mCurlParseWarning = new QLabel(curlPage);
    mCurlParseWarning->setStyleSheet(QStringLiteral("color:#b00;"));
    mCurlParseWarning->setWordWrap(true);
    mCurlParseWarning->hide();
    curlLayout->addWidget(mCurlParseWarning);
    mCurlTabIndex = mTabs->addTab(curlPage, tr("cURL"));

    root->addWidget(mTabs, 1);

    // ---- Bottom button row (shared) ----
    auto *bottomRow = new QHBoxLayout;
    mSaveButton  = new QPushButton(tr("Save…"), this);
    mLoadButton  = new QPushButton(tr("Load…"), this);
    mResetButton = new QPushButton(tr("Reset"), this);
    mSaveButton->setToolTip(
        tr("Save this request as a cURL text file (reusable preset)."));
    mLoadButton->setToolTip(
        tr("Load a request from a cURL text file."));
    mResetButton->setToolTip(
        tr("Restore the built-in default GET (with default headers)."));
    bottomRow->addWidget(mSaveButton);
    bottomRow->addWidget(mLoadButton);
    bottomRow->addStretch(1);
    bottomRow->addWidget(mResetButton);
    root->addLayout(bottomRow);

    connect(mAddHeaderButton, &QPushButton::clicked,
            this, &HttpRequestConfigWidget::onAddHeaderClicked);
    connect(mSaveButton, &QPushButton::clicked,
            this, &HttpRequestConfigWidget::saveToFile);
    connect(mLoadButton, &QPushButton::clicked,
            this, &HttpRequestConfigWidget::loadFromFile);
    connect(mResetButton, &QPushButton::clicked,
            this, &HttpRequestConfigWidget::resetToDefaults);
    connect(mMethodCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HttpRequestConfigWidget::onMethodChanged);
    connect(mTabs, &QTabWidget::currentChanged,
            this, &HttpRequestConfigWidget::onTabChanged);
}

HttpRequestConfigWidget::HeaderRow
HttpRequestConfigWidget::addHeaderRow(const QString &key, const QString &value)
{
    HeaderRow row;
    row.rowWidget = new QWidget;
    row.rowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *rowLayout = new QHBoxLayout(row.rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    row.keyEdit = new QLineEdit(row.rowWidget);
    row.keyEdit->setPlaceholderText(tr("Header-Name"));
    row.keyEdit->setText(key);
    // Widths that comfortably fit "Accept-Encoding" and
    // "gzip, deflate" respectively at default font size.
    row.keyEdit->setMinimumWidth(180);
    auto *completer = new QCompleter(commonHeaderNames(), row.keyEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    row.keyEdit->setCompleter(completer);

    row.valueEdit = new QLineEdit(row.rowWidget);
    row.valueEdit->setPlaceholderText(tr("value"));
    row.valueEdit->setText(value);
    row.valueEdit->setMinimumWidth(240);

    row.removeButton = new QToolButton(row.rowWidget);
    row.removeButton->setText(QStringLiteral("×"));
    row.removeButton->setToolTip(tr("Remove this header"));
    row.removeButton->setAutoRaise(false);

    rowLayout->addWidget(row.keyEdit, 2);
    rowLayout->addWidget(row.valueEdit, 3);
    rowLayout->addWidget(row.removeButton, 0);

    // Insert BEFORE the trailing stretch so rows stack top-to-bottom.
    // count() includes the stretch, so count()-1 targets the stretch
    // and pushes it down.
    const int insertAt = mHeadersLayout->count() - 1;
    mHeadersLayout->insertWidget(insertAt, row.rowWidget);
    mHeaderRows.append(row);

    // Sender-based deletion so we can find the right row when the
    // user clicks a button on any row.
    connect(row.removeButton, &QToolButton::clicked,
            this, &HttpRequestConfigWidget::onRemoveHeaderRowClicked);
    return row;
}

void HttpRequestConfigWidget::onAddHeaderClicked()
{
    HeaderRow row = addHeaderRow();
    row.keyEdit->setFocus();
    emit configChanged();
}

void HttpRequestConfigWidget::onRemoveHeaderRowClicked()
{
    QObject *btn = sender();
    for (int i = 0; i < mHeaderRows.size(); ++i)
        {
            if (mHeaderRows.at(i).removeButton == btn)
                {
                    QWidget *w = mHeaderRows.at(i).rowWidget;
                    mHeaderRows.removeAt(i);
                    mHeadersLayout->removeWidget(w);
                    w->deleteLater();
                    emit configChanged();
                    return;
                }
        }
}

void HttpRequestConfigWidget::clearHeaderRows()
{
    for (const HeaderRow &row : mHeaderRows)
        {
            mHeadersLayout->removeWidget(row.rowWidget);
            row.rowWidget->deleteLater();
        }
    mHeaderRows.clear();
}

void HttpRequestConfigWidget::onMethodChanged(int)
{
    updateBodyEnabledState();
    emit configChanged();
}

void HttpRequestConfigWidget::updateBodyEnabledState()
{
    // GET and HEAD carry no body by protocol semantics. DELETE and
    // PATCH occasionally do - leave those enabled.
    HttpRequestConfig::Method m = static_cast<HttpRequestConfig::Method>(
        mMethodCombo->currentData().toInt());
    bool bodyMakesSense = (m != HttpRequestConfig::Get)
                          && (m != HttpRequestConfig::Head);
    mBodyEdit->setEnabled(bodyMakesSense);
}

HttpRequestConfig HttpRequestConfigWidget::readFormConfig() const
{
    HttpRequestConfig c;
    c.method = static_cast<HttpRequestConfig::Method>(
        mMethodCombo->currentData().toInt());
    for (const HeaderRow &row : mHeaderRows)
        {
            const QString name = row.keyEdit->text().trimmed();
            if (name.isEmpty()) continue;
            c.headers.append(qMakePair(name, row.valueEdit->text()));
        }
    c.body = mBodyEdit->toPlainText().toUtf8();
    // URL rides through on mFormUrl - not exposed as a form widget,
    // but remembered from setConfig / cURL-tab edits so tab switches
    // and OK don't silently drop it.
    c.url = mFormUrl;
    return c;
}

void HttpRequestConfigWidget::populateForm(const HttpRequestConfig &config)
{
    // Method
    int idx = mMethodCombo->findData(config.method);
    if (idx < 0) idx = 0;
    QSignalBlocker mBlock(mMethodCombo);
    mMethodCombo->setCurrentIndex(idx);

    // Headers
    clearHeaderRows();
    for (const auto &h : config.headers)
        {
            addHeaderRow(h.first, h.second);
        }

    // Body
    QSignalBlocker bBlock(mBodyEdit);
    mBodyEdit->setPlainText(QString::fromUtf8(config.body));

    // Remember the URL that came in - readFormConfig replays it, so
    // switching to the cURL tab regenerates text WITH the URL in it.
    mFormUrl = config.url;

    updateBodyEnabledState();
}

HttpRequestConfig HttpRequestConfigWidget::config() const
{
    // Which tab is authoritative depends on where the user last
    // typed. If we're on the cURL tab, parse from there so an
    // uncommitted edit isn't silently dropped.
    if (mTabs->currentIndex() == mCurlTabIndex)
        {
            bool ok = false;
            HttpRequestConfig c = HttpRequestConfig::fromCurlText(
                mCurlEdit->toPlainText(), &ok);
            if (ok) return c;
            // Fall through: parse failed, hand back the form as a
            // best-effort. UX-wise the parse warning label under the
            // cURL editor already shows, so the user isn't surprised.
        }
    return readFormConfig();
}

void HttpRequestConfigWidget::setConfig(const HttpRequestConfig &config)
{
    populateForm(config);
    QSignalBlocker cBlock(mCurlEdit);
    mCurlEdit->setPlainText(config.toCurlText());
    mCurlParseWarning->hide();
    emit configChanged();
}

void HttpRequestConfigWidget::onTabChanged(int index)
{
    if (index == mCurlTabIndex)
        {
            // Form → cURL: regenerate the text from the current
            // form state so the user sees exactly what they'll send.
            HttpRequestConfig c = readFormConfig();
            QSignalBlocker cBlock(mCurlEdit);
            mCurlEdit->setPlainText(c.toCurlText());
            mCurlParseWarning->hide();
        }
    else if (index == mFormTabIndex)
        {
            // cURL → Form: parse the current text and update the
            // form. On parse failure leave the form as-is and show
            // the warning; don't lose the user's edit.
            bool ok = false;
            HttpRequestConfig c = HttpRequestConfig::fromCurlText(
                mCurlEdit->toPlainText(), &ok);
            if (!ok)
                {
                    mCurlParseWarning->setText(
                        tr("Couldn't parse this as a cURL command. "
                           "The form still shows the previous state."));
                    mCurlParseWarning->show();
                    // Bounce back to the cURL tab so the user sees
                    // the message and fixes the input.
                    QSignalBlocker tBlock(mTabs);
                    mTabs->setCurrentIndex(mCurlTabIndex);
                    return;
                }
            mCurlParseWarning->hide();
            // populateForm updates mFormUrl from c.url, so a URL the
            // user typed into the cURL tab survives the tab flip and
            // will be handed back by readFormConfig / config().
            populateForm(c);
        }
    emit configChanged();
}

void HttpRequestConfigWidget::resetToDefaults()
{
    setConfig(HttpRequestConfig::defaults());
}

void HttpRequestConfigWidget::saveToFile()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save request as cURL preset"),
        QDir::homePath(),
        tr("cURL preset (*.curl *.txt);;All files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(
                this, tr("Save failed"),
                tr("Could not open %1 for writing:\n%2")
                    .arg(path, f.errorString()));
            return;
        }
    f.write(config().toCurlText().toUtf8());
    f.write("\n");
}

void HttpRequestConfigWidget::loadFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load request from cURL preset"),
        QDir::homePath(),
        tr("cURL preset (*.curl *.txt);;All files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMessageBox::warning(
                this, tr("Load failed"),
                tr("Could not open %1 for reading:\n%2")
                    .arg(path, f.errorString()));
            return;
        }
    const QString text = QString::fromUtf8(f.readAll());
    bool ok = false;
    HttpRequestConfig c = HttpRequestConfig::fromCurlText(text, &ok);
    if (!ok)
        {
            QMessageBox::warning(
                this, tr("Load failed"),
                tr("The file at %1 doesn't parse as a cURL command.")
                    .arg(path));
            return;
        }
    setConfig(c);
}
