#include <boost/test/unit_test.hpp>

#include "boost_printers.hpp"

#include <cstring>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QHash>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

import biblerunner.api;

namespace
{
    constexpr char APP_KEY[] = "test-app-key";

    // QCoreApplication for QNetworkAccessManager and KConfig
    struct AppFixture
    {
        AppFixture() { qputenv("XDG_RUNTIME_DIR", tempDir.path().toUtf8()); }

        QTemporaryDir tempDir;
        // the process argv, as QCoreApplication wants it
        int argc = 1;
        char arg0[17] = "biblerunner-test";
        char *argv[2] = {arg0, nullptr};
        QCoreApplication app{argc, argv};
    };

    // true when the tested code quit() the loop itself, false on timeout
    bool waitForEventLoop(QEventLoop &loop, int timeoutMs)
    {
        QTimer watchdog;
        watchdog.setSingleShot(true);
        watchdog.setInterval(timeoutMs);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        watchdog.start();

        loop.exec();

        return watchdog.isActive();
    }

    // fabricated reply: a prepared status and body, no networking
    class StubReply final : public QNetworkReply
    {
    public:
        StubReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, int status,
                  QByteArray body, QObject *parent)
            : QNetworkReply(parent), body(std::move(body))
        {
            setUrl(request.url());
            setOperation(op);
            open(QIODevice::ReadOnly);
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
            setHeader(QNetworkRequest::ContentLengthHeader, body.size());
            // HTTP errors surface as transport errors, like with a real manager
            if (status != 200 && status != 204)
            {
                setError(QNetworkReply::UnknownContentError,
                         QStringLiteral("server replied: HTTP %1").arg(status));
            }
        }

        void finish() { Q_EMIT finished(); }

        void abort() final {}

    private:
        qint64 readData(char *data, qint64 maxSize) final
        {
            if (offset >= body.size())
            {
                return 0;
            }
            const qint64 count = qMin<qint64>(maxSize, body.size() - offset);
            std::memcpy(data, body.constData() + offset, static_cast<std::size_t>(count));
            offset += count;
            return count;
        }

        QByteArray body;
        qint64 offset = 0;
    };

    // hands out fabricated replies, records every request
    class StubNetwork final : public QNetworkAccessManager
    {
    public:
        struct RecordedRequest
        {
            QUrl url;
            QHash<QByteArray, QByteArray> headers; // lower case name -> value
        };

        // responses in queue order; a drained queue repeats the empty 200
        explicit StubNetwork(std::vector<std::pair<int, QByteArray>> responses = {},
                             QObject *parent = nullptr)
            : QNetworkAccessManager(parent), responses(std::move(responses))
        {
        }

        const RecordedRequest &request(int index) const { return requests.at(index); }
        int requestCount() const { return static_cast<int>(requests.size()); }

    protected:
        QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *) final
        {
            RecordedRequest &recorded = requests.emplace_back();
            recorded.url = request.url();
            for (const QByteArray &name : request.rawHeaderList())
            {
                recorded.headers.insert(name.toLower(), request.rawHeader(name));
            }

            const auto [status, body] = nextResponse();
            auto *reply = new StubReply(op, request, status, body, this);
            // queue finished() so it fires after the fetch connected
            QTimer::singleShot(0, reply, &StubReply::finish);
            return reply;
        }

    private:
        std::pair<int, QByteArray> nextResponse()
        {
            if (next < responses.size())
            {
                return responses[next++];
            }
            return {200, {}};
        }

        std::vector<std::pair<int, QByteArray>> responses;
        std::size_t next = 0;
        std::vector<RecordedRequest> requests;
    };

    // BOOST_REQUIRE aborts the case when the done callback never fires
    LanguagesResult runLanguages(StubNetwork &network)
    {
        QObject guard;
        QEventLoop loop;
        std::optional<LanguagesResult> result;
        fetchLanguages(&network, QLatin1String(APP_KEY), &guard,
                       [&result, &loop](LanguagesResult done)
                       {
                           result.emplace(std::move(done));
                           loop.quit();
                       });

        BOOST_REQUIRE(waitForEventLoop(loop, 5000));
        return std::move(*result);
    }

    // nullopt: done callback never fired (cancelled walk)
    std::optional<BiblesResult> runBibles(StubNetwork &network,
                                          const std::function<bool()> &cancelled = [] { return false; },
                                          int timeoutMs = 5000)
    {
        QObject guard;
        QEventLoop loop;
        std::optional<BiblesResult> result;
        fetchBibles(&network, QLatin1String(APP_KEY), &guard, cancelled,
                    [&result, &loop](BiblesResult done)
                    {
                        result.emplace(std::move(done));
                        loop.quit();
                    });

        if (!waitForEventLoop(loop, timeoutMs))
        {
            return std::nullopt;
        }
        return std::move(*result);
    }
} // namespace

BOOST_TEST_GLOBAL_FIXTURE(AppFixture);

BOOST_AUTO_TEST_SUITE(api)

BOOST_AUTO_TEST_CASE(languages_parsed_from_response)
{
    StubNetwork network({{200, QByteArrayLiteral(
                                       "{\"data\":["
                                       "{\"id\":\"eng\",\"localized_name\":\"English\"},"
                                       "{\"id\":\"deu\",\"localized_name\":\"German\"},"
                                       "{\"id\":\"frr\"}" // no name: kept with an empty name
                                       "]}")}});

    const LanguagesResult result = runLanguages(network);

    BOOST_TEST(result.error.isEmpty());
    BOOST_TEST(result.languages.size() == 3);
    BOOST_TEST(result.languages.value(QLatin1String("eng")).toString() == QStringLiteral("English"));
    BOOST_TEST(result.languages.value(QLatin1String("deu")).toString() == QStringLiteral("German"));
    BOOST_TEST(result.languages.value(QLatin1String("frr")).toString() == QString());

    // the request carries the API path, the app key and the JSON headers
    const StubNetwork::RecordedRequest &hit = network.request(0);
    BOOST_TEST(hit.url.path() == QStringLiteral("/v1/languages"));
    BOOST_TEST(hit.headers.value(QByteArrayLiteral("x-yvp-app-key")) == QByteArray(APP_KEY));
    BOOST_TEST(hit.headers.value(QByteArrayLiteral("accept")).toLower() ==
               QByteArrayLiteral("application/json"));
    BOOST_TEST(hit.headers.value(QByteArrayLiteral("accept-language")) == QByteArrayLiteral("en"));
}

BOOST_AUTO_TEST_CASE(languages_http_error_reports_error)
{
    StubNetwork network({{500, {}}});

    const LanguagesResult result = runLanguages(network);

    BOOST_TEST(!result.error.isEmpty());
    BOOST_TEST(result.languages.isEmpty());
}

BOOST_AUTO_TEST_CASE(bibles_walk_follows_page_tokens)
{
    StubNetwork network({
        {200, QByteArrayLiteral("{\"data\":["
                                "{\"id\":2095,\"title\":\"King Jim\",\"abbreviation\":\"KJ\",\"language_tag\":\"en\"},"
                                "{\"id\":\"3040\",\"title\":\"Some Standard Bible\",\"abbreviation\":\"SSB\",\"language_tag\":\"en\"}"
                                "],\"next_page_token\":\"cursor-2\"}")},
        {200, QByteArrayLiteral("{\"data\":["
                                "{\"id\":\"3012\",\"title\":\"Open Friendly Bible\",\"abbreviation\":\"OFB\",\"language_tag\":\"de\"}"
                                "]}")},
    });

    const std::optional<BiblesResult> result = runBibles(network);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->error.isEmpty());
    BOOST_REQUIRE(result->bibles.size() == 3);
    // a JSON number id turns into its string form
    BOOST_TEST(result->bibles[0].id == QLatin1String("2095"));
    BOOST_TEST(result->bibles[0].title == QStringLiteral("King Jim"));
    BOOST_TEST(result->bibles[0].abbreviation == QLatin1String("KJ"));
    BOOST_TEST(result->bibles[0].languageTag == QLatin1String("en"));
    BOOST_TEST(result->bibles[1].id == QLatin1String("3040"));
    BOOST_TEST(result->bibles[2].id == QLatin1String("3012"));

    // two pages, the second one requested via the page token
    BOOST_TEST(network.requestCount() == 2);
    BOOST_TEST(network.request(0).url.path() == QStringLiteral("/v1/bibles"));
    BOOST_TEST(network.request(1).url.query().contains(QLatin1String("page_token=cursor-2")));
}

BOOST_AUTO_TEST_CASE(bibles_single_page_without_token)
{
    StubNetwork network({{200, QByteArrayLiteral("{\"data\":[{\"id\":\"3040\"}]}")}});

    const std::optional<BiblesResult> result = runBibles(network);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->error.isEmpty());
    BOOST_REQUIRE(result->bibles.size() == 1);
    BOOST_TEST(result->bibles[0].id == QLatin1String("3040"));
    BOOST_TEST(network.requestCount() == 1);
    BOOST_TEST(network.request(0).url.path() == QStringLiteral("/v1/bibles"));
}

BOOST_AUTO_TEST_CASE(bibles_cancelled_walk_reports_nothing)
{
    StubNetwork network({{200, QByteArrayLiteral("{\"data\":[]}")}});

    // superseded generation: the walk stops silently, done never fires
    const std::optional<BiblesResult> result = runBibles(network, [] { return true; }, 1000);

    BOOST_TEST(!result.has_value());
    BOOST_TEST(network.requestCount() == 1);
}

BOOST_AUTO_TEST_CASE(bibles_204_reports_success_without_entries)
{
    StubNetwork network({{204, {}}});

    const std::optional<BiblesResult> result = runBibles(network);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->error.isEmpty());
    BOOST_TEST(result->bibles.isEmpty());
}

BOOST_AUTO_TEST_CASE(bibles_http_error_reports_error)
{
    StubNetwork network({{500, {}}});

    const std::optional<BiblesResult> result = runBibles(network);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(!result->error.isEmpty());
    BOOST_TEST(result->bibles.isEmpty());
}

BOOST_AUTO_TEST_CASE(bibles_non_json_response_reports_error)
{
    StubNetwork network({{200, QByteArrayLiteral("plain text, no json")}});

    const std::optional<BiblesResult> result = runBibles(network);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->error.contains(QLatin1String("unexpected")));
    BOOST_TEST(result->bibles.isEmpty());
}

BOOST_AUTO_TEST_SUITE_END()
