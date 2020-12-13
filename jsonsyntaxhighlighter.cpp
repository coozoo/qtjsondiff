#include "jsonsyntaxhighlighter.h"

JsonSyntaxHighlighter::JsonSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    keywordFormat.setForeground(Qt::white);

    QStringList keywordPatterns;
    keywordPatterns << "\"" << "\'";

    Q_FOREACH (const QString &pattern, keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // TODO: colors to be configured: https://github.com/coozoo/qtjsondiff/issues/16
    jsonKeyFormat.setForeground(QBrush(Qt::red));
    jsonKeyFormat.setFontWeight(QFont::Bold);

    rule.pattern = QRegularExpression("\".*\"(?=:)");
    rule.format = jsonKeyFormat;
    highlightingRules.append(rule);

    jsonValueKeywordFormat.setForeground(QBrush(Qt::green));

    rule.pattern = QRegularExpression("(?= ?)true|false|null(?= +|,)");
    rule.format = jsonValueKeywordFormat;
    highlightingRules.append(rule);
}

void JsonSyntaxHighlighter::highlightBlock(const QString &text)
{
    Q_FOREACH (const HighlightingRule &rule, highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);

        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
