/* Author: Yuriy Kuzin
 */
#ifndef HTTPREQUESTCONFIG_H
#define HTTPREQUESTCONFIG_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QPair>
#include <QNetworkRequest>

// A single container's URL request configuration: method, custom
// headers, request body, and an optional URL (used when the config
// text came from a devtools "Copy as cURL" paste that carries the
// URL along). The canonical serialization is cURL command text, so
// the same string works as a preset file on disk, the CLI argument
// file, and the "raw" tab in the config dialog.
class HttpRequestConfig
{
public:
    enum Method
    {
        Get,
        Post,
        Put,
        Patch,
        Delete,
        Head
    };

    Method method = Get;
    QList<QPair<QString, QString> > headers;
    QByteArray body;
    // Optional. When fromCurlText picks up a URL from the pasted
    // command, we surface it here so the caller can decide whether
    // to also overwrite the container's URL field. Empty means the
    // parsed text didn't carry a URL.
    QString url;

    // The "factory GET" - matches today's hardcoded getData()
    // behavior so behavior on first run is identical to pre-feature
    // code and Reset produces something you can inspect and edit.
    static HttpRequestConfig defaults();

    // True when this config is byte-for-byte the factory GET. Used
    // to decide whether to badge the toolbar button.
    bool isDefault() const;

    // Applies method-independent bits (raw headers) onto a
    // QNetworkRequest. The URL and method are the caller's job.
    void applyTo(QNetworkRequest &request) const;

    // Case-insensitive header lookup. Returns empty QString if not
    // present. Used by callers that want to know whether the config
    // already sets, e.g., Content-Type.
    QString headerValue(const QString &name) const;

    // Human-readable method name ("GET", "POST", …). Used by the
    // dispatch in QJsonContainer::getData and by toCurlText.
    static QString methodName(Method m);
    // Reverse lookup. Case-insensitive. Returns Get and sets *ok
    // to false when the name doesn't match any known method.
    static Method methodFromName(const QString &name, bool *ok = nullptr);

    // Serialize to a cURL command string. Uses single-quoted values,
    // splits headers onto continuation lines with backslash+newline
    // so the output copy-pastes cleanly back into a shell.
    QString toCurlText() const;

    // Best-effort parse of a cURL command. Handles:
    //  - -X / --request
    //  - -H / --header
    //  - -d / --data / --data-raw / --data-binary
    //  - single and double quoted values, backslash line continuations
    //  - a bare URL argument anywhere in the command
    // Unknown flags are silently skipped. On unrecoverable parse
    // failure returns defaults() and sets *ok to false.
    static HttpRequestConfig fromCurlText(const QString &text, bool *ok = nullptr);
};

#endif // HTTPREQUESTCONFIG_H
