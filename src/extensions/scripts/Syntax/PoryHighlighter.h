#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QVector>

class QTextDocument;

class PoryHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit PoryHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightingRule> m_highlightingRules;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_labelFormat;
    QTextCharFormat m_directiveFormat;
    QTextCharFormat m_keywordFormat;

    void initializeRules();
};

