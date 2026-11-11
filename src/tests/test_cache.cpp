#include <boost/test/unit_test.hpp>

#include "boost_printers.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <optional>

import biblerunner.cache;

#include "modules/catalog.h"

namespace
{
    // per-case XDG cache dir; QStandardPaths re-reads the env on every call,
    // so the fixture path and catalogCachePath() stay in sync
    struct CacheDir
    {
        CacheDir()
        {
            BOOST_REQUIRE(dir.isValid());
            qputenv("XDG_CACHE_HOME", dir.path().toUtf8());
        }

        QString catalogPath() const
        {
            return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                   + QLatin1String("/biblerunner/catalog.json");
        }

        QTemporaryDir dir;
    };

    CatalogCache sampleCatalog()
    {
        CatalogCache cache;
        cache.languages.insert(QLatin1String("eng"), QStringLiteral("English"));
        cache.languages.insert(QLatin1String("deu"), QStringLiteral("German"));
        cache.bibles.append(BibleEntry{QLatin1String("2095"), QStringLiteral("King James Version"),
                                       QLatin1String("KJV"), QLatin1String("eng")});
        cache.bibles.append(BibleEntry{QLatin1String("3040"), QStringLiteral("Open Friendly Bible"),
                                       QLatin1String("OFB"), QLatin1String("deu")});
        cache.fetchedAt = QDateTime::currentSecsSinceEpoch();
        return cache;
    }
} // namespace

BOOST_AUTO_TEST_SUITE(catalog_cache)

BOOST_FIXTURE_TEST_CASE(empty_without_cache_file, CacheDir)
{
    const CatalogCache cache = readCatalogCache();
    BOOST_TEST(cache.bibles.isEmpty());
    BOOST_TEST(cache.languages.isEmpty());
    BOOST_TEST(cache.fetchedAt == 0);
    BOOST_TEST(catalogCacheStale(cache));
}

BOOST_FIXTURE_TEST_CASE(write_then_read_roundtrip, CacheDir)
{
    const CatalogCache written = sampleCatalog();
    writeCatalogCache(written);
    BOOST_TEST(QFile::exists(catalogPath()));

    const CatalogCache cache = readCatalogCache();
    BOOST_REQUIRE(cache.bibles.size() == written.bibles.size());
    for (int i = 0; i < written.bibles.size(); ++i)
    {
        BOOST_TEST_CONTEXT("bible " << written.bibles[i].id.toStdString())
        {
            BOOST_TEST(cache.bibles[i].id == written.bibles[i].id);
            BOOST_TEST(cache.bibles[i].title == written.bibles[i].title);
            BOOST_TEST(cache.bibles[i].abbreviation == written.bibles[i].abbreviation);
            BOOST_TEST(cache.bibles[i].languageTag == written.bibles[i].languageTag);
        }
    }
    BOOST_TEST(cache.languages.size() == 2);
    BOOST_TEST(cache.languages.value(QLatin1String("eng")).toString() == QStringLiteral("English"));
    BOOST_TEST(cache.languages.value(QLatin1String("deu")).toString() == QStringLiteral("German"));
    BOOST_TEST(cache.fetchedAt == written.fetchedAt);
    // fresh timestamp from the sample: not stale
    BOOST_TEST(!catalogCacheStale(cache));
}

BOOST_FIXTURE_TEST_CASE(read_ignores_idless_entries, CacheDir)
{
    CatalogCache written = sampleCatalog();
    written.bibles.prepend(BibleEntry{ {}, QStringLiteral("no id"), {}, {} });

    writeCatalogCache(written);

    const CatalogCache cache = readCatalogCache();
    BOOST_TEST(cache.bibles.size() == 2);
    BOOST_TEST(cache.bibles[0].id == QLatin1String("2095"));
}

BOOST_FIXTURE_TEST_CASE(read_tolerates_corrupt_or_unusable_file, CacheDir)
{
    BOOST_REQUIRE(QDir().mkpath(QFileInfo(catalogPath()).path()));
    {
        QFile file(catalogPath());
        BOOST_REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("* not json *");
    }

    CatalogCache cache = readCatalogCache();
    BOOST_TEST(cache.bibles.isEmpty());
    BOOST_TEST(cache.fetchedAt == 0);

    // json, but without the bibles array
    {
        QFile file(catalogPath());
        BOOST_REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("{\"fetched_at\": 123}");
    }

    cache = readCatalogCache();
    BOOST_TEST(cache.bibles.isEmpty());
    BOOST_TEST(cache.fetchedAt == 0);
}

BOOST_AUTO_TEST_CASE(staleness_rules)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    constexpr qint64 day = 24 * 3600;

    CatalogCache cache = sampleCatalog();

    // month old: stale
    cache.fetchedAt = now - (30 * day + 60);
    BOOST_TEST(catalogCacheStale(cache));

    // a day before the threshold: fresh
    cache.fetchedAt = now - (29 * day);
    BOOST_TEST(!catalogCacheStale(cache));

    // never fetched: stale, even with bibles
    cache.fetchedAt = 0;
    BOOST_TEST(catalogCacheStale(cache));

    // no bibles: stale, even when fresh
    cache.fetchedAt = now;
    cache.bibles.clear();
    BOOST_TEST(catalogCacheStale(cache));
}

BOOST_AUTO_TEST_SUITE_END()
