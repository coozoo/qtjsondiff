#ifndef JSONSYNTAXHIGHLIGHTER_H
#define JSONSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>


class JsonSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    JsonSyntaxHighlighter(QTextDocument *parent);

protected:
    void highlightBlock(const QString &text);

private:
  struct HighlightingRule {
      QRegularExpression pattern;
      QTextCharFormat format;
  };
  QList<HighlightingRule> highlightingRules;

  QTextCharFormat keywordFormat;
  QTextCharFormat jsonKeyFormat;
  QTextCharFormat jsonValueTextFormat;
  QTextCharFormat jsonValueKeywordFormat;
};

#endif // JSONSYNTAXHIGHLIGHTER_H
