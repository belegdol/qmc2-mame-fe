#ifndef ECMASCRIPTHIGHLIGHTER_H
#define ECMASCRIPTHIGHLIGHTER_H

#include <QSyntaxHighlighter>

class ECMAScriptHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit ECMAScriptHighlighter(QTextDocument *parent);

protected:
	virtual void highlightBlock(const QString &);

private:
	struct HighlightingRule
	{
		QRegularExpression pattern;
		QTextCharFormat format;
	};
	QVector<HighlightingRule> mHighlightingRules;

	QRegularExpression mMultiLineCommentStartExpression;
	QRegularExpression mMultiLineCommentEndExpression;
	QRegularExpression mSingleLineCommentExpression;

	QTextCharFormat mKeywordFormat;
	QTextCharFormat mSingleLineCommentFormat;
	QTextCharFormat mMultiLineCommentFormat;
	QTextCharFormat mQuotationFormat;
	QTextCharFormat mFunctionFormat;
	QTextCharFormat mScriptEngineFormat;
};

#endif // ECMASCRIPTHIGHLIGHTER_H
