/* Author: Yuriy Kuzin
 */
#include "httprequestconfig.h"

#include <QStringList>
#include <QUrl>

HttpRequestConfig HttpRequestConfig::defaults()
{
    HttpRequestConfig c;
    c.method = Get;
    // Mirrors the pre-feature getData() so first-run behavior is
    // identical for anyone who never opens the dialog.
    c.headers.append(qMakePair(QStringLiteral("Accept-Language"),
                               QStringLiteral("en-US,en;q=0.8")));
    c.headers.append(qMakePair(QStringLiteral("Cache-Control"),
                               QStringLiteral("no-cache")));
    c.headers.append(qMakePair(QStringLiteral("Accept-Encoding"),
                               QStringLiteral("gzip, deflate")));
    return c;
}

bool HttpRequestConfig::isDefault() const
{
    const HttpRequestConfig d = defaults();
    return method == d.method
           && headers == d.headers
           && body == d.body
           && url.isEmpty();
}

void HttpRequestConfig::applyTo(QNetworkRequest &request) const
{
    for (const auto &h : headers)
        {
            request.setRawHeader(h.first.toUtf8(), h.second.toUtf8());
        }
}

QString HttpRequestConfig::headerValue(const QString &name) const
{
    for (const auto &h : headers)
        {
            if (h.first.compare(name, Qt::CaseInsensitive) == 0)
                return h.second;
        }
    return QString();
}

QString HttpRequestConfig::methodName(Method m)
{
    switch (m)
        {
        case Get:    return QStringLiteral("GET");
        case Post:   return QStringLiteral("POST");
        case Put:    return QStringLiteral("PUT");
        case Patch:  return QStringLiteral("PATCH");
        case Delete: return QStringLiteral("DELETE");
        case Head:   return QStringLiteral("HEAD");
        }
    return QStringLiteral("GET");
}

HttpRequestConfig::Method HttpRequestConfig::methodFromName(const QString &name, bool *ok)
{
    const QString upper = name.trimmed().toUpper();
    if (ok) *ok = true;
    if (upper == QLatin1String("GET"))    return Get;
    if (upper == QLatin1String("POST"))   return Post;
    if (upper == QLatin1String("PUT"))    return Put;
    if (upper == QLatin1String("PATCH"))  return Patch;
    if (upper == QLatin1String("DELETE")) return Delete;
    if (upper == QLatin1String("HEAD"))   return Head;
    if (ok) *ok = false;
    return Get;
}

// Wrap a value in single quotes for a POSIX shell. Any embedded
// single quote is emitted as the classic '\'' trick so the value
// round-trips through the shell's tokenizer.
static QString shellSingleQuote(const QString &v)
{
    QString escaped = v;
    escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString HttpRequestConfig::toCurlText() const
{
    QStringList lines;
    QString head = QStringLiteral("curl -X ") + methodName(method);
    if (!url.isEmpty())
        {
            head += QLatin1Char(' ') + shellSingleQuote(url);
        }
    lines << head;

    for (const auto &h : headers)
        {
            lines << QStringLiteral("  -H ")
                     + shellSingleQuote(h.first + QStringLiteral(": ") + h.second);
        }

    if (!body.isEmpty())
        {
            // --data-raw so the value never triggers curl's @file
            // shorthand - matches devtools' "Copy as cURL" output.
            lines << QStringLiteral("  --data-raw ")
                     + shellSingleQuote(QString::fromUtf8(body));
        }

    return lines.join(QStringLiteral(" \\\n"));
}

// Tokenize a shell-ish command line. Understands:
//  - single quotes: literal, no escapes
//  - double quotes: backslash escapes next char
//  - backslash outside quotes: escapes next char (a backslash before
//    a newline is a line continuation - collapses to a space)
//  - whitespace separates tokens
// Not a full shell parser - good enough for cURL commands as they
// appear in devtools "Copy as cURL" and hand-written config files.
static QStringList tokenizeCurl(const QString &input, bool *ok)
{
    QStringList tokens;
    QString cur;
    bool inSingle = false;
    bool inDouble = false;
    bool haveToken = false;

    if (ok) *ok = true;

    const int n = input.size();
    for (int i = 0; i < n; ++i)
        {
            QChar c = input.at(i);

            if (inSingle)
                {
                    if (c == QLatin1Char('\''))
                        {
                            inSingle = false;
                        }
                    else
                        {
                            cur.append(c);
                        }
                    continue;
                }
            if (inDouble)
                {
                    if (c == QLatin1Char('\\') && i + 1 < n)
                        {
                            QChar nxt = input.at(i + 1);
                            // In double quotes the shell only
                            // escapes a handful of chars; for
                            // anything else the backslash is kept
                            // literal. Good enough for cURL text.
                            if (nxt == QLatin1Char('"')
                                || nxt == QLatin1Char('\\')
                                || nxt == QLatin1Char('$')
                                || nxt == QLatin1Char('`')
                                || nxt == QLatin1Char('\n'))
                                {
                                    if (nxt != QLatin1Char('\n'))
                                        cur.append(nxt);
                                    ++i;
                                }
                            else
                                {
                                    cur.append(c);
                                }
                        }
                    else if (c == QLatin1Char('"'))
                        {
                            inDouble = false;
                        }
                    else
                        {
                            cur.append(c);
                        }
                    continue;
                }

            // Unquoted mode.
            if (c == QLatin1Char('\\') && i + 1 < n)
                {
                    QChar nxt = input.at(i + 1);
                    if (nxt == QLatin1Char('\n'))
                        {
                            // line continuation: consume both,
                            // treat as whitespace
                            ++i;
                            if (haveToken)
                                {
                                    tokens.append(cur);
                                    cur.clear();
                                    haveToken = false;
                                }
                            continue;
                        }
                    cur.append(nxt);
                    haveToken = true;
                    ++i;
                    continue;
                }
            if (c == QLatin1Char('\''))
                {
                    inSingle = true;
                    haveToken = true;
                    continue;
                }
            if (c == QLatin1Char('"'))
                {
                    inDouble = true;
                    haveToken = true;
                    continue;
                }
            if (c.isSpace())
                {
                    if (haveToken)
                        {
                            tokens.append(cur);
                            cur.clear();
                            haveToken = false;
                        }
                    continue;
                }

            cur.append(c);
            haveToken = true;
        }

    if (inSingle || inDouble)
        {
            if (ok) *ok = false;
            return QStringList();
        }
    if (haveToken) tokens.append(cur);

    return tokens;
}

HttpRequestConfig HttpRequestConfig::fromCurlText(const QString &text, bool *ok)
{
    HttpRequestConfig cfg;
    // Not defaults() - user typed something in, so we don't want
    // silent Accept-Language / Cache-Control / Accept-Encoding
    // headers sneaking into their request.
    cfg.method = Get;

    bool tokOk = true;
    QStringList tokens = tokenizeCurl(text, &tokOk);
    if (!tokOk)
        {
            if (ok) *ok = false;
            return defaults();
        }
    if (ok) *ok = true;

    if (tokens.isEmpty())
        {
            // Empty input is a valid "no-op" config, but callers
            // shouldn't get an empty method list - hand back
            // defaults for consistency.
            return defaults();
        }

    // Drop a leading "curl" if present.
    int idx = 0;
    if (tokens.at(0).compare(QLatin1String("curl"), Qt::CaseInsensitive) == 0)
        idx = 1;

    bool methodSetExplicitly = false;
    bool bodySeen = false;

    while (idx < tokens.size())
        {
            const QString &t = tokens.at(idx);

            auto needValue = [&](const QString &flag) -> QString
            {
                if (idx + 1 >= tokens.size()) return QString();
                Q_UNUSED(flag);
                return tokens.at(++idx);
            };

            if (t == QLatin1String("-X") || t == QLatin1String("--request"))
                {
                    QString val = needValue(t);
                    bool mOk = false;
                    Method m = methodFromName(val, &mOk);
                    if (mOk)
                        {
                            cfg.method = m;
                            methodSetExplicitly = true;
                        }
                }
            else if (t == QLatin1String("-H") || t == QLatin1String("--header"))
                {
                    QString val = needValue(t);
                    int colon = val.indexOf(QLatin1Char(':'));
                    if (colon > 0)
                        {
                            QString name = val.left(colon).trimmed();
                            QString value = val.mid(colon + 1).trimmed();
                            cfg.headers.append(qMakePair(name, value));
                        }
                }
            else if (t == QLatin1String("-d")
                     || t == QLatin1String("--data")
                     || t == QLatin1String("--data-raw")
                     || t == QLatin1String("--data-binary")
                     || t == QLatin1String("--data-ascii"))
                {
                    QString val = needValue(t);
                    cfg.body = val.toUtf8();
                    bodySeen = true;
                }
            else if (t == QLatin1String("--url"))
                {
                    QString val = needValue(t);
                    cfg.url = val;
                }
            else if (t == QLatin1String("-A") || t == QLatin1String("--user-agent"))
                {
                    QString val = needValue(t);
                    cfg.headers.append(qMakePair(QStringLiteral("User-Agent"), val));
                }
            else if (t == QLatin1String("-e") || t == QLatin1String("--referer"))
                {
                    QString val = needValue(t);
                    cfg.headers.append(qMakePair(QStringLiteral("Referer"), val));
                }
            else if (t.startsWith(QLatin1Char('-')))
                {
                    // Unknown flag. Skip it and swallow its value if
                    // one appears to follow - better than
                    // misinterpreting the value as a URL.
                    if (idx + 1 < tokens.size()
                        && !tokens.at(idx + 1).startsWith(QLatin1Char('-')))
                        {
                            // Only swallow if it doesn't look like a
                            // URL - a bare URL is what we most want
                            // to preserve.
                            const QString &nxt = tokens.at(idx + 1);
                            if (!nxt.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
                                && !nxt.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
                                {
                                    ++idx;
                                }
                        }
                }
            else
                {
                    // Positional argument. Treat as URL if it looks
                    // URL-ish or if we haven't seen a URL yet.
                    if (cfg.url.isEmpty())
                        {
                            cfg.url = t;
                        }
                }

            ++idx;
        }

    // curl's own semantic: -d without an explicit -X implies POST.
    if (bodySeen && !methodSetExplicitly && cfg.method == Get)
        {
            cfg.method = Post;
        }

    return cfg;
}
