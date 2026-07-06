#include <QtTest>
#include <QPlainTextEdit>
#include <QTabWidget>
#include "httprequestconfig.h"
#include "httprequestconfigwidget.h"

class TestHttpRequestConfig : public QObject
{
    Q_OBJECT
private slots:
    void defaultsRoundTrip();
    void parseCurlFromDevtools();
    void parsePostImpliedByData();
    void parseSurvivesLineContinuations();
    void parseHandlesDoubleQuotes();
    void parseKeepsUrlEvenAfterUnknownFlag();
    void emitEscapesSingleQuotesInValues();
    void headerLookupIsCaseInsensitive();

    // Widget-level smoke tests.
    void widgetStartsWithDefaults();
    void widgetSetConfigRoundTrip();
    void widgetTabSwitchAppliesCurlEdit();
};

void TestHttpRequestConfig::defaultsRoundTrip()
{
    HttpRequestConfig d = HttpRequestConfig::defaults();
    QCOMPARE(d.method, HttpRequestConfig::Get);
    QVERIFY(d.isDefault());

    // Round-trip: defaults() → cURL text → parse → equal shape.
    bool ok = false;
    HttpRequestConfig parsed = HttpRequestConfig::fromCurlText(d.toCurlText(), &ok);
    QVERIFY(ok);
    QCOMPARE(parsed.method, HttpRequestConfig::Get);
    QCOMPARE(parsed.headers.size(), d.headers.size());
    for (int i = 0; i < parsed.headers.size(); ++i)
    {
        QCOMPARE(parsed.headers.at(i).first,  d.headers.at(i).first);
        QCOMPARE(parsed.headers.at(i).second, d.headers.at(i).second);
    }
}

void TestHttpRequestConfig::parseCurlFromDevtools()
{
    // Shape of what Chrome devtools emits for "Copy as cURL".
    const QString text = QStringLiteral(
        "curl 'https://api.example.com/foo' \\\n"
        "  -X POST \\\n"
        "  -H 'Authorization: Bearer XYZ' \\\n"
        "  -H 'Content-Type: application/json' \\\n"
        "  --data-raw '{\"k\":\"v\"}'"
    );
    bool ok = false;
    HttpRequestConfig c = HttpRequestConfig::fromCurlText(text, &ok);
    QVERIFY(ok);
    QCOMPARE(c.method, HttpRequestConfig::Post);
    QCOMPARE(c.url, QStringLiteral("https://api.example.com/foo"));
    QCOMPARE(c.headers.size(), 2);
    QCOMPARE(c.headers.at(0).first,  QStringLiteral("Authorization"));
    QCOMPARE(c.headers.at(0).second, QStringLiteral("Bearer XYZ"));
    QCOMPARE(c.headers.at(1).first,  QStringLiteral("Content-Type"));
    QCOMPARE(c.body, QByteArray("{\"k\":\"v\"}"));
}

void TestHttpRequestConfig::parsePostImpliedByData()
{
    // -d without -X should promote GET to POST, matching curl.
    bool ok = false;
    HttpRequestConfig c = HttpRequestConfig::fromCurlText(
        QStringLiteral("curl https://x -d 'hello'"), &ok);
    QVERIFY(ok);
    QCOMPARE(c.method, HttpRequestConfig::Post);
    QCOMPARE(c.body, QByteArray("hello"));
}

void TestHttpRequestConfig::parseSurvivesLineContinuations()
{
    bool ok = false;
    HttpRequestConfig c = HttpRequestConfig::fromCurlText(
        QStringLiteral("curl -X PUT \\\n  https://x \\\n  -H 'A: 1'"), &ok);
    QVERIFY(ok);
    QCOMPARE(c.method, HttpRequestConfig::Put);
    QCOMPARE(c.url, QStringLiteral("https://x"));
    QCOMPARE(c.headers.size(), 1);
    QCOMPARE(c.headers.at(0).second, QStringLiteral("1"));
}

void TestHttpRequestConfig::parseHandlesDoubleQuotes()
{
    bool ok = false;
    HttpRequestConfig c = HttpRequestConfig::fromCurlText(
        QStringLiteral("curl -X POST -H \"Content-Type: application/json\" https://x"),
        &ok);
    QVERIFY(ok);
    QCOMPARE(c.headers.size(), 1);
    QCOMPARE(c.headers.at(0).first, QStringLiteral("Content-Type"));
    QCOMPARE(c.headers.at(0).second, QStringLiteral("application/json"));
    QCOMPARE(c.url, QStringLiteral("https://x"));
}

void TestHttpRequestConfig::parseKeepsUrlEvenAfterUnknownFlag()
{
    // --compressed is legit curl but not one we track. The URL that
    // follows must survive - that's the whole point of the "swallow
    // value only if it doesn't look URL-ish" branch in the parser.
    bool ok = false;
    HttpRequestConfig c = HttpRequestConfig::fromCurlText(
        QStringLiteral("curl --compressed https://x/y"), &ok);
    QVERIFY(ok);
    QCOMPARE(c.url, QStringLiteral("https://x/y"));
}

void TestHttpRequestConfig::emitEscapesSingleQuotesInValues()
{
    HttpRequestConfig c;
    c.method = HttpRequestConfig::Post;
    c.url = QStringLiteral("https://x");
    c.headers.append(qMakePair(QStringLiteral("X-Note"),
                               QStringLiteral("it's tricky")));
    c.body = QByteArray("{\"a\":'b'}");
    const QString curl = c.toCurlText();

    // Round-trip: emitter output must parse back to an equivalent
    // config (single quotes and all).
    bool ok = false;
    HttpRequestConfig round = HttpRequestConfig::fromCurlText(curl, &ok);
    QVERIFY2(ok, qPrintable(QStringLiteral("emitted cURL didn't parse: ") + curl));
    QCOMPARE(round.method, HttpRequestConfig::Post);
    QCOMPARE(round.url, QStringLiteral("https://x"));
    QCOMPARE(round.headers.size(), 1);
    QCOMPARE(round.headers.at(0).second, QStringLiteral("it's tricky"));
    QCOMPARE(round.body, QByteArray("{\"a\":'b'}"));
}

void TestHttpRequestConfig::headerLookupIsCaseInsensitive()
{
    HttpRequestConfig c;
    c.headers.append(qMakePair(QStringLiteral("Content-Type"),
                               QStringLiteral("application/json")));
    QCOMPARE(c.headerValue(QStringLiteral("content-type")),
             QStringLiteral("application/json"));
    QCOMPARE(c.headerValue(QStringLiteral("CONTENT-TYPE")),
             QStringLiteral("application/json"));
    QVERIFY(c.headerValue(QStringLiteral("Nope")).isEmpty());
}

void TestHttpRequestConfig::widgetStartsWithDefaults()
{
    HttpRequestConfigWidget w;
    QVERIFY(w.config().isDefault());
}

void TestHttpRequestConfig::widgetSetConfigRoundTrip()
{
    HttpRequestConfigWidget w;
    HttpRequestConfig c;
    c.method = HttpRequestConfig::Put;
    c.headers.append(qMakePair(QStringLiteral("Content-Type"),
                               QStringLiteral("application/json")));
    c.body = QByteArray("{\"foo\":1}");
    w.setConfig(c);

    HttpRequestConfig round = w.config();
    QCOMPARE(round.method, HttpRequestConfig::Put);
    QCOMPARE(round.headers.size(), 1);
    QCOMPARE(round.headers.at(0).first,  QStringLiteral("Content-Type"));
    QCOMPARE(round.headers.at(0).second, QStringLiteral("application/json"));
    QCOMPARE(round.body, QByteArray("{\"foo\":1}"));
}

void TestHttpRequestConfig::widgetTabSwitchAppliesCurlEdit()
{
    // Uses findChildren to reach into the widget so we don't have
    // to friend the test class. Not glamorous - good enough for a
    // smoke test that proves the tab-sync wiring is alive.
    HttpRequestConfigWidget w;
    auto plainEdits = w.findChildren<QPlainTextEdit *>();
    QVERIFY(!plainEdits.isEmpty());

    QPlainTextEdit *curlEdit = nullptr;
    for (QPlainTextEdit *e : plainEdits)
        {
            if (e->placeholderText().contains(QStringLiteral("curl")))
                {
                    curlEdit = e;
                    break;
                }
        }
    QVERIFY(curlEdit);

    auto *tabs = w.findChild<QTabWidget *>();
    QVERIFY(tabs);

    // Switch to cURL tab, replace text, switch back - the widget
    // should now report the pasted config.
    tabs->setCurrentIndex(1);
    curlEdit->setPlainText(QStringLiteral(
        "curl -X POST 'https://x' -H 'Authorization: Bearer T' -d 'body'"));
    tabs->setCurrentIndex(0);

    HttpRequestConfig c = w.config();
    QCOMPARE(c.method, HttpRequestConfig::Post);
    QCOMPARE(c.headers.size(), 1);
    QCOMPARE(c.headers.at(0).first,  QStringLiteral("Authorization"));
    QCOMPARE(c.headers.at(0).second, QStringLiteral("Bearer T"));
    QCOMPARE(c.body, QByteArray("body"));
}

QTEST_MAIN(TestHttpRequestConfig)
#include "test_httprequest.moc"
