#include <boost/test/unit_test.hpp>

#include "boost_printers.hpp"

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

import biblerunner.config;

namespace
{
    // per-case XDG config dir; KSharedConfig caches by path, so a fresh
    // directory also gives a fresh krunnerrc
    struct ConfigDir
    {
        ConfigDir()
        {
            BOOST_REQUIRE(dir.isValid());
            qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        }

        QString rcPath() const
        {
            return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                   + QLatin1String("/krunnerrc");
        }

        QTemporaryDir dir;
    };
} // namespace

BOOST_AUTO_TEST_SUITE(config)

BOOST_AUTO_TEST_CASE(is_configured_matrix)
{
    Config config;
    BOOST_TEST(!config.isConfigured());

    config.appKey = QStringLiteral("key-1");
    BOOST_TEST(!config.isConfigured());

    config.bibleId = QStringLiteral("2095");
    BOOST_TEST(config.isConfigured());
}

BOOST_FIXTURE_TEST_CASE(load_unconfigured_defaults, ConfigDir)
{
    const Config config = loadConfig();
    BOOST_TEST(config.appKey.isEmpty());
    BOOST_TEST(config.bibleId.isEmpty());
    BOOST_TEST(!config.isConfigured());
}

BOOST_FIXTURE_TEST_CASE(save_then_load_roundtrip, ConfigDir)
{
    Config written;
    written.appKey = QStringLiteral("key-from-test");
    written.bibleId = QStringLiteral("2076");
    saveConfig(written);

    // the entries land in the runner's own config group file
    BOOST_TEST(QFile::exists(rcPath()));

    const Config config = loadConfig();
    BOOST_TEST(config.appKey == QStringLiteral("key-from-test"));
    BOOST_TEST(config.bibleId == QStringLiteral("2076"));
    BOOST_TEST(config.isConfigured());
}

BOOST_FIXTURE_TEST_CASE(save_overwrites_previous_values, ConfigDir)
{
    Config written;
    written.appKey = QStringLiteral("first-key");
    written.bibleId = QStringLiteral("2095");
    saveConfig(written);

    written.appKey = QStringLiteral("second-key");
    written.bibleId = QStringLiteral("3000");
    saveConfig(written);

    const Config config = loadConfig();
    BOOST_TEST(config.appKey == QStringLiteral("second-key"));
    BOOST_TEST(config.bibleId == QStringLiteral("3000"));
}

BOOST_FIXTURE_TEST_CASE(empty_bible_id_keeps_key, ConfigDir)
{
    Config written;
    written.appKey = QStringLiteral("key-only");
    written.bibleId = QString();
    saveConfig(written);

    const Config config = loadConfig();
    BOOST_TEST(config.appKey == QStringLiteral("key-only"));
    BOOST_TEST(config.bibleId.isEmpty());
    BOOST_TEST(!config.isConfigured());
}

BOOST_AUTO_TEST_SUITE_END()
