module;

#include <optional>
#include <string>
#include <vector>

#include <boost/algorithm/string/join.hpp>

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

    // "3", "16", "18" -> "3.16-18" style strings built from reference parts
    std::string joinParts(const std::vector<std::string> &parts, const char *separator)
    {
        return boost::algorithm::join(parts, separator);
    }
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
    const QStringList split = text.split(separatorRegex);
    if (split.size() > 3)
        return std::nullopt;

    // chapter [, verse [, last verse]] -> {"3"}, {"3", "16"}, {"3", "16-18"}
    std::vector<std::string> reference{split[0].toStdString()};
    if (split.size() > 1)
    {
        std::vector<std::string> verses;
        for (qsizetype i = 1; i < split.size(); ++i)
            verses.push_back(split[i].toStdString());
        reference.push_back(joinParts(verses, "-"));
    }

    const std::string bookCode = book.toStdString();
    const QString suffix = QString::fromStdString(joinParts(reference, "."));
    text = QString::fromStdString(joinParts(reference, ":"));
    const QString url = QStringLiteral("https://www.bible.com/bible/%1/%2.%3").arg(bibleId, book, suffix);
    const QString passage = QString::fromStdString(bookCode + "." + joinParts(reference, "."));

    return QueryMatchInfo{book, text, passage, QUrl(url)};
}
