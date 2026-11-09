module;

#include <functional>
#include <mutex>
#include <optional>
#include <utility>

#include <QDebug>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QtNetwork/QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "catalog.h"

export module biblerunner.api;

export struct VerseInfo
{
    QString title; // "John 3:16 BSB"
    QString text;
};

namespace
{
    constexpr char API_BASE_URL[] = "https://api.youversion.com/v1";

    constexpr int CATALOG_PAGE_SIZE = 99; // API max page_size
    constexpr int CATALOG_TIMEOUT_MS = 15000;

    // GET /v1/bibles/{id}
    QUrl bibleInfoUrl(const QString &bibleId)
    {
        return QUrl(QStringLiteral("%1/bibles/%2").arg(API_BASE_URL, bibleId));
    }

    // GET /v1/bibles/{id}/passages/{usfm}?format=text
    QUrl biblePassagesUrl(const QString &bibleId, const QString &passage)
    {
        return QUrl(QStringLiteral("%1/bibles/%2/passages/%3?format=text").arg(API_BASE_URL, bibleId, passage));
    }

    // GET /v1/languages
    QUrl languagesUrl()
    {
        QUrl url(QStringLiteral("%1/languages").arg(API_BASE_URL));
        QUrlQuery query;
        // only two fields => page_size=* allowed (whole list in one response)
        query.addQueryItem(QLatin1String("bibles_available"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("id"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("localized_name"));
        query.addQueryItem(QLatin1String("page_size"), QStringLiteral("*"));
        url.setQuery(query);
        return url;
    }

    // GET /v1/bibles, paged
    QUrl biblesCatalogUrl(const QString &pageToken)
    {
        QUrl url(QStringLiteral("%1/bibles").arg(API_BASE_URL));
        QUrlQuery query;
        // all_available=true: whole catalog, not only the versions licensed
        // to this key; >2 fields => page_size=* not allowed, so we paginate
        query.addQueryItem(QLatin1String("all_available"), QStringLiteral("true"));
        query.addQueryItem(QLatin1String("language_ranges[]"), QStringLiteral("*"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("id"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("title"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("abbreviation"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("language_tag"));
        query.addQueryItem(QLatin1String("page_size"), QString::number(CATALOG_PAGE_SIZE));
        if (!pageToken.isEmpty())
        {
            query.addQueryItem(QLatin1String("page_token"), pageToken);
        }
        url.setQuery(query);
        return url;
    }
} // namespace

namespace
{
    QNetworkReply *submitGet(QNetworkAccessManager *network, const QUrl &url, const QString &appKey)
    {
        QNetworkRequest request(url);
        request.setRawHeader(QByteArrayLiteral("X-YVP-App-Key"), appKey.toUtf8());
        request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
        // English display names regardless of locale
        request.setRawHeader(QByteArrayLiteral("Accept-Language"), QByteArrayLiteral("en"));
        request.setTransferTimeout(CATALOG_TIMEOUT_MS);
        return network->get(request);
    }

    // 204 counts as success (empty body)
    bool replyOk(QNetworkReply *reply, QString *error)
    {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status != 204)
        {
            *error = reply->errorString();
            return false;
        }
        if (status != 0 && status != 200 && status != 204)
        {
            *error = QStringLiteral("HTTP %1").arg(status);
            return false;
        }
        return true;
    }

    QJsonDocument replyJson(QNetworkReply *reply)
    {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
            return QJsonDocument();
        }
        return doc;
    }
} // namespace

export struct LanguagesResult
{
    QJsonObject languages; // language id -> display name
    QString error;         // empty on success
};

export struct BiblesResult
{
    QVector<BibleEntry> bibles; // partial when the walk hit an error
    QString error;              // empty on success
};

// GET /v1/languages; done() is invoked once from reply->finished(), bound to guard
export void fetchLanguages(QNetworkAccessManager *network, const QString &appKey, QObject *guard,
                           const std::function<void(LanguagesResult)> &done)
{
    QNetworkReply *reply = submitGet(network, languagesUrl(), appKey);
    QObject::connect(reply, &QNetworkReply::finished, guard, [reply, done]()
            {
        reply->deleteLater();

        QString error;
        if (!replyOk(reply, &error))
        {
            done(LanguagesResult{QJsonObject(), std::move(error)});
            return;
        }

        QJsonObject languages;
        const QJsonDocument doc = replyJson(reply);
        if (doc.isObject())
        {
            const QJsonArray entries = doc.object().value(QLatin1String("data")).toArray();
            for (const QJsonValue &value : entries)
            {
                const QJsonObject language = value.toObject();
                const QString id = language.value(QLatin1String("id")).toString();
                const QString name = language.value(QLatin1String("localized_name")).toString();
                if (!id.isEmpty())
                {
                    languages.insert(id, name);
                }
            }
        }
        done(LanguagesResult{std::move(languages), QString()}); });
}

namespace
{
    // one page of the /v1/bibles walk; a superseded fetch stops silently
    void fetchBiblesPage(QNetworkAccessManager *network, const QString &appKey, QObject *guard,
                         const std::function<bool()> &cancelled, const QString &pageToken,
                         QVector<BibleEntry> entries, const std::function<void(BiblesResult)> &done)
    {
        QNetworkReply *reply = submitGet(network, biblesCatalogUrl(pageToken), appKey);
        QObject::connect(reply, &QNetworkReply::finished, guard,
                         [=, entries = std::move(entries), done = done]() mutable
                {
                    reply->deleteLater();
                    if (cancelled())
                    {
                        return;
                    }

                    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    QString error;
                    if (!replyOk(reply, &error))
                    {
                        done(BiblesResult{std::move(entries), std::move(error)});
                        return;
                    }
                    if (status == 204)
                    {
                        done(BiblesResult{std::move(entries), QString()});
                        return;
                    }

                    const QJsonDocument doc = replyJson(reply);
                    if (!doc.isObject())
                    {
                        done(BiblesResult{std::move(entries), QStringLiteral("unexpected response body from the YouVersion API")});
                        return;
                    }

                    const QJsonObject page = doc.object();
                    const QJsonArray entriesJson = page.value(QLatin1String("data")).toArray();
                    for (const QJsonValue &value : entriesJson)
                    {
                        const QJsonObject version = value.toObject();
                        const QJsonValue idValue = version.value(QLatin1String("id"));
                        const QString id = idValue.isString()
                                               ? idValue.toString()
                                               : QString::number(idValue.toVariant().toLongLong());
                        if (id.isEmpty())
                        {
                            continue;
                        }
                        BibleEntry entry;
                        entry.id = id;
                        entry.title = version.value(QLatin1String("title")).toString();
                        entry.abbreviation = version.value(QLatin1String("abbreviation")).toString();
                        entry.languageTag = version.value(QLatin1String("language_tag")).toString();
                        entries.append(entry);
                    }

                    const QString nextToken = page.value(QLatin1String("next_page_token")).toVariant().toString();
                    if (!nextToken.isEmpty())
                    {
                        fetchBiblesPage(network, appKey, guard, cancelled, nextToken, std::move(entries), done);
                        return;
                    }
                    done(BiblesResult{std::move(entries), QString()}); });
    }
} // namespace

// paged walk over the whole /v1/bibles catalog; reports success and errors alike
export void fetchBibles(QNetworkAccessManager *network, const QString &appKey, QObject *guard,
                        const std::function<bool()> &cancelled,
                        const std::function<void(BiblesResult)> &done)
{
    fetchBiblesPage(network, appKey, guard, cancelled, QString(), QVector<BibleEntry>(), done);
}

namespace
{
    // blocking GET for the runner thread
    QJsonDocument apiGet(const QString &appKey, const QUrl &url)
    {
        QEventLoop eventLoop;
        QNetworkAccessManager manager;
        QObject::connect(&manager, &QNetworkAccessManager::finished, &eventLoop, &QEventLoop::quit);

        QNetworkReply *reply = submitGet(&manager, url, appKey);
        eventLoop.exec();

        QString error;
        if (!replyOk(reply, &error))
        {
            qWarning() << "BibleRunner: request to" << url.toString() << "failed:" << error;
            delete reply;
            return QJsonDocument();
        }

        const QJsonDocument doc = replyJson(reply);
        delete reply;
        if (!doc.isObject())
        {
            qWarning() << "BibleRunner: request to" << url.toString() << "returned non-JSON response";
            return QJsonDocument();
        }
        return doc;
    }

    QString bibleAbbreviation(const QString &appKey, const QString &bibleId)
    {
        // single-slot cache
        static std::mutex cacheMutex;
        std::lock_guard<std::mutex> lock(cacheMutex);
        static QString cachedBibleId;
        static QString cachedAbbreviation;

        if (cachedBibleId == bibleId)
        {
            return cachedAbbreviation;
        }

        QJsonDocument doc = apiGet(appKey, bibleInfoUrl(bibleId));
        QString abbreviation;
        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            abbreviation = obj.value(QLatin1String("localized_abbreviation")).toString();
            if (abbreviation.isEmpty())
            {
                abbreviation = obj.value(QLatin1String("abbreviation")).toString();
            }
        }

        // cached even when empty (don't hammer the API)
        cachedBibleId = bibleId;
        cachedAbbreviation = abbreviation;
        return abbreviation;
    }
} // namespace

export std::optional<VerseInfo> fetchVerse(const QString &appKey, const QString &bibleId, const QString &passage)
{
    QJsonDocument doc = apiGet(appKey, biblePassagesUrl(bibleId, passage));
    if (!doc.isObject())
    {
        return std::nullopt;
    }

    QJsonObject obj = doc.object();
    QString content = obj.value(QLatin1String("content")).toString();
    if (content.isEmpty())
    {
        return std::nullopt;
    }

    VerseInfo info;
    info.title = obj.value(QLatin1String("reference")).toString();
    if (info.title.isEmpty())
    {
        info.title = passage;
    }

    QString abbreviation = bibleAbbreviation(appKey, bibleId);
    if (!abbreviation.isEmpty())
    {
        info.title.append(QLatin1Char(' ')).append(abbreviation);
    }

    info.text = content.trimmed();

    return info;
}
