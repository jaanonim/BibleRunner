module;

#include <optional>

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "books.h"

export module biblerunner.query;

export struct QueryMatchInfo
{
    QString book;    // USFM book code
    QString text;    // human text, e.g. "3:16-18"
    QString passage; // USFM passage for the API, e.g. "JHN.3.16", "JHN.3", "JHN.3.16-18"
    QUrl url;        // bible.com browse URL for the configured bible
};

namespace
{
    const QRegularExpression linkRegex(QLatin1String(R"(([123]\s?)?\p{L}+\s?\d{1,3}([:,.]\s?\d{1,3}(-\d{1,3})?)?)"));
    const QRegularExpression bookRegex(QLatin1String(R"(([123]\s?)?\p{L}+)"));
    const QRegularExpression separatorRegex(QLatin1String(R"([-:,.]+)"));
    const QRegularExpression spaceRegex(QLatin1String("\\s"));
} // namespace

export std::optional<QueryMatchInfo> parseQuery(const QString &rawQuery, const QString &bibleId)
{
    QString query = rawQuery;

    if (query.startsWith(QLatin1Char('?')))
        return std::nullopt;

    query = query.trimmed();

    QRegularExpressionMatch re_match = linkRegex.match(query);
    if (!re_match.hasMatch())
        return std::nullopt;

    QString text = re_match.captured(0).trimmed();
    if (text.length() != query.length())
        return std::nullopt;

    re_match = bookRegex.match(text);
    if (!re_match.hasMatch())
        return std::nullopt;

    QString inBook = re_match.captured(0);
    text.remove(0, inBook.length());
    QString book = inBook.toLower().trimmed().remove(spaceRegex);
    std::string search = lookupBook(book.toStdString());

    if (search.length() == 0)
        return std::nullopt;
    book = QString::fromStdString(search);

    text = text.trimmed();
    QStringList split = text.split(separatorRegex);
    QString url = QLatin1String("https://www.bible.com/bible/%1/%2.").arg(bibleId, book);
    QString passage = book;
    if (split.length() == 1)
    {
        text = split[0];
        url.append(split[0]);
        passage.append(QLatin1Char('.')).append(split[0]);
    }
    else if (split.length() == 2)
    {
        text = split[0] + QLatin1Char(':') + split[1];
        url.append(split[0] + QLatin1Char('.') + split[1]);
        passage.append(QLatin1Char('.')).append(split[0]).append(QLatin1Char('.')).append(split[1]);
    }
    else if (split.length() == 3)
    {
        text = split[0] + QLatin1Char(':') + split[1] + QLatin1Char('-') + split[2];
        url.append(split[0] + QLatin1Char('.') + split[1] + QLatin1Char('-') + split[2]);
        passage.append(QLatin1Char('.'))
            .append(split[0])
            .append(QLatin1Char('.'))
            .append(split[1])
            .append(QLatin1Char('-'))
            .append(split[2]);
    }
    else
    {
        return std::nullopt;
    }

    return QueryMatchInfo{book, text, passage, QUrl(url)};
}
