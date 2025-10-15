#include "extensions/scripts/Syntax/PoryHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextCharFormat>
#include <QTextDocument>

#include <utility>

PoryHighlighter::PoryHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    initializeRules();
}

void PoryHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : std::as_const(m_highlightingRules)) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Handle multi-line comments manually.
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(QLatin1String("/*"));

    while (startIndex >= 0) {
        int endIndex = text.indexOf(QLatin1String("*/"), startIndex + 2);
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + 2;
        }
        setFormat(startIndex, commentLength, m_commentFormat);
        startIndex = text.indexOf(QLatin1String("/*"), startIndex + commentLength);
    }

    if (previousBlockState() == 1) {
        int endIndex = text.indexOf(QLatin1String("*/"));
        if (endIndex == -1) {
            setFormat(0, text.length(), m_commentFormat);
            setCurrentBlockState(1);
        } else {
            setFormat(0, endIndex + 2, m_commentFormat);
            setCurrentBlockState(0);
        }
    } else {
        setCurrentBlockState(0);
    }
}

void PoryHighlighter::initializeRules()
{
    m_highlightingRules.clear();

    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(79, 120, 201));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString keywordPattern = QStringLiteral("\\b(?:if|else|while|switch|case|default|return|setflag|clearflag|setvar|addv"
                                                 "ar|subvar|message|goto|call|callnative|lock|release|wait|applymovement|waitm"
                                                 "ovement|special|specialvar|storemoney|takemoney|warp|warpsilent|warpteleport|p"
                                                 "layse|playfanfare|playsound|playsong|fadesong|fanfare|giveitem|takeitem)\\b");
    m_highlightingRules.append({QRegularExpression(keywordPattern), keywordFormat});
    m_keywordFormat = keywordFormat;

    m_stringFormat.setForeground(QColor(196, 142, 72));
    m_highlightingRules.append({QRegularExpression(QStringLiteral("\"([^\\\n]|\\.)*\"")), m_stringFormat});
    m_highlightingRules.append({QRegularExpression(QStringLiteral("'([^\\\n]|\\.)*'")), m_stringFormat});

    m_numberFormat.setForeground(QColor(111, 149, 83));
    m_highlightingRules.append({QRegularExpression(QStringLiteral("\\b0[xX][0-9A-Fa-f]+\\b")), m_numberFormat});
    m_highlightingRules.append({QRegularExpression(QStringLiteral("\\b[0-9]+\\b")), m_numberFormat});

    m_labelFormat.setForeground(QColor(143, 115, 200));
    m_highlightingRules.append({QRegularExpression(QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*):")), m_labelFormat});

    m_directiveFormat.setForeground(QColor(180, 90, 90));
    m_highlightingRules.append({QRegularExpression(QStringLiteral("^\\s*#(?:include|define|ifdef|ifndef|endif|elif|else|pragma|s"
                                                                  "cript)\\b")), m_directiveFormat});

    m_commentFormat.setForeground(QColor(125, 125, 125));
    m_highlightingRules.append({QRegularExpression(QStringLiteral("//[^\n]*")), m_commentFormat});
}

