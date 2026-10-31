module;

#include <optional>

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <KLocalizedString>

export module biblerunner.query;

import biblerunner.data.books;

export struct QueryMatchInfo
{
    QString book;
    QString text;
    QUrl url;
};

export struct VerseInfo
{
    QString title;
    QString abbreviation;
    QString text;
};

namespace
{
    const QRegularExpression linkRegex(QLatin1String(R"(([123]\s?)?\p{L}+\s?\d{1,3}([:,.]\s?\d{1,3}(-\d{1,3})?)?)"));
    const QRegularExpression bookRegex(QLatin1String(R"(([123]\s?)?\p{L}+)"));
    const QRegularExpression separatorRegex(QLatin1String(R"([-:,.]+)"));
    const QRegularExpression spaceRegex(QLatin1String("\\s"));
    const QRegularExpression htmlRegex(QLatin1String(
        R"(<script\s*id="__NEXT_DATA__"\s*type="application\/json"\s*>.+?(?=<\/script>\s*<\/body>\s*<\/html>))"));
    const QRegularExpression htmlDelRegex(QLatin1String(
        R"(<script\s*id="__NEXT_DATA__"\s*type="application\/json"\s*>)"));

    QString bookUrl(const QString &book)
    {
        QString base = QLatin1String("https://www.bible.com/bible");
        return i18n("%1/%2/%3.", base, QLatin1String("2095"), book);
    }

    QString getTextOfVerse(const QUrl &url)
    {
        // create custom temporary event loop on stack
        QEventLoop eventLoop;

        // "quit()" the event-loop, when the network request "finished()"
        QNetworkAccessManager mgr;
        QObject::connect(&mgr, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));

        QNetworkRequest req(url);
        QNetworkReply *reply = mgr.get(req);
        eventLoop.exec();

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray data = reply->readAll();
            delete reply;
            return QString::fromUtf8(data);
        }
        else
        {
            delete reply;
            return QLatin1String("");
        }
    }
} // namespace

export std::optional<QueryMatchInfo> parseQuery(const QString &rawQuery)
{
    QString query = rawQuery;

    if (query.startsWith(QLatin1Char('?')))
    {
        return std::nullopt;
    }

    query = query.trimmed();

    QRegularExpressionMatch re_match = linkRegex.match(query);
    if (!re_match.hasMatch())
    {
        return std::nullopt;
    }

    QString text = re_match.captured(0).trimmed();
    if (text.length() != query.length())
    {
        return std::nullopt;
    }

    re_match = bookRegex.match(text);
    if (!re_match.hasMatch())
    {
        return std::nullopt;
    }

    QString inBook = re_match.captured(0);
    text.remove(0, inBook.length());
    QString book = inBook.toLower().trimmed().remove(spaceRegex);
    if (std::string search = lookupBook(book.toStdString()); search.length() != 0)
    {
        book = QString::fromStdString(search);
    }
    else
    {
        return std::nullopt;
    }

    text = text.trimmed();
    QStringList split = text.split(separatorRegex);
    QString url = bookUrl(book);
    if (split.length() == 1)
    {
        text = split[0];
        url.append(split[0]);
    }
    else if (split.length() == 2)
    {
        text = split[0] + QLatin1Char(':') + split[1];
        url.append(split[0] + QLatin1Char('.') + split[1]);
    }
    else if (split.length() == 3)
    {
        text = split[0] + QLatin1Char(':') + split[1] + QLatin1Char('-') + split[2];
        url.append(split[0] + QLatin1Char('.') + split[1] + QLatin1Char('-') + split[2]);
    }
    else
    {
        return std::nullopt;
    }

    return QueryMatchInfo{book, text, QUrl(url)};
}

export std::optional<VerseInfo> fetchVerse(const QUrl &url)
{
    QString data = getTextOfVerse(url);

    if (data.length() < 1)
    {
        return std::nullopt;
    }

    QRegularExpressionMatch re_match = htmlRegex.match(data);
    if (!re_match.hasMatch())
    {
        return std::nullopt;
    }

    QString json_str = re_match.captured(0);
    json_str.remove(htmlDelRegex);

    QJsonObject json = QJsonDocument::fromJson(json_str.toUtf8()).object();

    QJsonObject stuff = json.value(QLatin1String("props")).toObject().value(QLatin1String("pageProps")).toObject();

    if (stuff.value(QLatin1String("type")).toVariant().toString() != QLatin1String("verse"))
    {
        return std::nullopt;
    }

    VerseInfo info;
    info.title =
        stuff.value(QLatin1String("referenceTitle")).toObject().value(QString(QLatin1String("title"))).toVariant().toString();
    info.abbreviation = stuff.value(QLatin1String("version"))
                            .toObject()
                            .value(QString(QLatin1String("local_abbreviation")))
                            .toVariant()
                            .toString();

    QJsonArray verses_array = stuff.value(QLatin1String("verses")).toArray();
    QString verses = QString::fromUtf8("");
    for (auto verse : verses_array)
    {
        verses += verse.toObject().value(QLatin1String("content")).toVariant().toString();
        verses += QString::fromUtf8(" ");
    }
    info.text = verses.trimmed();

    return info;
}
