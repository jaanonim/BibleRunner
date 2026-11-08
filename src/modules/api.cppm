// The YouVersion Platform API layer (https://api.youversion.com/v1), shared
// by the runner plugin and its configuration module: building the endpoint
// URLs, submitting the requests and handling the replies all live in this
// module, so no call site holds the API base URL or the request headers.
// Besides the blocking fetchVerse() used by the runner thread it also runs
// the whole version catalog fetch of the configuration module (one languages
// request plus one request per page of the paged bibles walk), reported
// through callbacks so the configuration UI never has to block.

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
    QString title; // human reference + translation abbreviation, e.g. "John 3:16 BSB"
    QString text;  // verse text
};

namespace
{
    constexpr char API_BASE_URL[] = "https://api.youversion.com/v1";

    constexpr int CATALOG_PAGE_SIZE = 99; // page_size must be between 1 and 99
    constexpr int CATALOG_TIMEOUT_MS = 15000;

    // GET /v1/bibles/{id}: details of one version, used for its abbreviation
    QUrl bibleInfoUrl(const QString &bibleId)
    {
        return QUrl(QStringLiteral("%1/bibles/%2").arg(API_BASE_URL, bibleId));
    }

    // GET /v1/bibles/{id}/passages/{usfm}?format=text: text of one passage
    QUrl biblePassagesUrl(const QString &bibleId, const QString &passage)
    {
        return QUrl(QStringLiteral("%1/bibles/%2/passages/%3?format=text").arg(API_BASE_URL, bibleId, passage));
    }

    // GET /v1/languages: languages that carry bible content, with display names
    QUrl languagesUrl()
    {
        QUrl url(QStringLiteral("%1/languages").arg(API_BASE_URL));
        QUrlQuery query;
        // bibles_available=true restricts to languages that carry bible content.
        // Only two fields are requested, which unlocks page_size=* (the whole
        // list in one response; inside the fields parameter three or more fields
        // force pagination).
        query.addQueryItem(QLatin1String("bibles_available"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("id"));
        query.addQueryItem(QStringLiteral("fields[]"), QLatin1String("localized_name"));
        query.addQueryItem(QLatin1String("page_size"), QStringLiteral("*"));
        url.setQuery(query);
        return url;
    }

    // GET /v1/bibles: the whole platform catalog of versions, walked one page of
    // CATALOG_PAGE_SIZE entries at a time by passing back the page token
    QUrl biblesCatalogUrl(const QString &pageToken)
    {
        QUrl url(QStringLiteral("%1/bibles").arg(API_BASE_URL));
        QUrlQuery query;
        // all_available=true lists the whole catalog, not only the versions
        // licensed to this app key. Four fields are requested, so page_size=* is
        // not allowed: the list is walked page by page instead (a little over 30
        // requests of 99 entries each).
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
    // Requested with the standard YouVersion Platform headers
    QNetworkReply *submitGet(QNetworkAccessManager *network, const QUrl &url, const QString &appKey)
    {
        QNetworkRequest request(url);
        request.setRawHeader(QByteArrayLiteral("X-YVP-App-Key"), appKey.toUtf8());
        request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
        // English language display names, independent of the host process locale
        request.setRawHeader(QByteArrayLiteral("Accept-Language"), QByteArrayLiteral("en"));
        request.setTransferTimeout(CATALOG_TIMEOUT_MS);
        return network->get(request);
    }

    // HTTP status check of a reply; 204 counts as an empty-but-successful body
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

    // Body of a successful reply as JSON (empty document on HTTP 204)
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

// Result of the languages request of one catalog fetch
export struct LanguagesResult
{
    QJsonObject languages; // language id (BCP 47) -> display name
    QString error;         // empty on success; a body that is not the expected
                           // JSON object also yields an empty map without error
};

// Result of the paged bibles walk of one catalog fetch
export struct BiblesResult
{
    QVector<BibleEntry> bibles; // versions of every page fetched so far
                                // (partial when the walk ended with an error)
    QString error;              // empty on success
};

// GET /v1/languages (non-blocking): languages that carry bible content, with
// display names. done() is invoked once, from the reply's finished() signal;
// the connection is made to guard, so nothing runs after guard is destroyed.
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
    // One page of the /v1/bibles walk: the entries are carried down the chain,
    // and either the walk result is reported or the next page is requested by
    // following next_page_token. When cancelled() returns true (the caller has
    // started a newer fetch), the walk stops and done() is never called; the
    // connection to guard makes every handler equally safe after destruction.
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
                        // plain error text, displayed by the caller
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

// GET /v1/bibles (non-blocking): walks the whole platform catalog of versions
// one page at a time, following next_page_token, and reports every entry
// accumulated when the walk ends (also after an error, with the pages fetched
// so far). cancelled() is checked before every page request, so a superseded
// fetch stops issuing requests and never reports; like the languages request,
// the reply connections are made to guard.
export void fetchBibles(QNetworkAccessManager *network, const QString &appKey, QObject *guard,
                        const std::function<bool()> &cancelled,
                        const std::function<void(BiblesResult)> &done)
{
    fetchBiblesPage(network, appKey, guard, cancelled, QString(), QVector<BibleEntry>(), done);
}

namespace
{
    // Blocking GET + JSON parse used by the runner thread: a custom temporary
    // event loop is run on the stack and quit() when the request finishes()
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
        // single-slot cache: bibleId -> abbreviation
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

        // cache even if empty to avoid hammering the API; refetches when bibleId changes
        cachedBibleId = bibleId;
        cachedAbbreviation = abbreviation;
        return abbreviation;
    }
} // namespace

// GET the text of one passage plus the reference the API resolved it to,
// titled with the version abbreviation
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
