//
// Created by jaanonim on 01.02.24.
//

#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include "biblerunner.h"
#include <QIcon>
#include <QProcess>
#include <QApplication>
#include <QClipboard>
#include <QtCore>
#include <QObject>
#include <mutex>

import biblerunner.api;
import biblerunner.query;
import biblerunner.config;

namespace
{
    std::mutex configMutex;
    Config config;

    Config loadConfigGuarded()
    {
        std::lock_guard lock(configMutex);
        return config;
    }

    void reloadConfig()
    {
        std::lock_guard lock(configMutex);
        config = loadConfig();
    }
} // namespace

BibleRunner::BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList & /*args*/)
    : AbstractRunner(parent, data) {}

void BibleRunner::reloadConfiguration()
{
    reloadConfig();
}

void BibleRunner::init()
{
    reloadConfiguration();
    connect(this, &KRunner::AbstractRunner::prepare, this, []() {});
    connect(this, &KRunner::AbstractRunner::teardown, this, []() {});
}

void BibleRunner::match(KRunner::RunnerContext &context)
{
    Config loaded = loadConfigGuarded();

    QString bibleId = QLatin1String(DEFAULT_BIBLE_ID); // fallback default translation
    if (!loaded.bibleId.isEmpty())
    {
        bibleId = loaded.bibleId;
    }

    auto info = parseQuery(context.query(), bibleId);
    if (!info)
    {
        return;
    }

    QList<KRunner::QueryMatch> matches;
    makeBrowserMatch(&matches, info->book, info->text, info->url.toString());
    if (loaded.isConfigured())
        makeCopyMatch(&matches, info->passage, info->url.toString());
    else
        makeSetKeyMatch(&matches);
    context.addMatches(matches);
}

void BibleRunner::makeBrowserMatch(QList<KRunner::QueryMatch> *matches, const QString &book, const QString &text,
                                   const QString &url)
{
    KRunner::QueryMatch match(this);
    match.setText(i18n("Open %1 %2", book, text));
    match.setSubtext(i18n("Open Bible in browser"));
    match.setIcon(QIcon::fromTheme(QStringLiteral("bible-icon")));

    QUrl qurl;
    qurl.setUrl(url);
    QList<QUrl> qulr_list;
    qulr_list.append(qurl);
    match.setUrls(qulr_list);

    match.setData(MatchAction::Browser);
    match.setId(i18n("%1_browser", url));
    matches->append(match);
}

void BibleRunner::makeCopyMatch(QList<KRunner::QueryMatch> *matches, const QString &passage, const QString &url)
{
    // Only called with a configured runner (gated in match()); the guarded copy is
    // taken here so a concurrent reloadConfiguration() cannot tear down the Config
    // while the network request uses it.
    const Config loaded = loadConfigGuarded();
    auto info = fetchVerse(loaded.appKey, loaded.bibleId, passage);
    if (!info)
    {
        return;
    }

    KRunner::QueryMatch match(this);
    match.setText(i18n("%1 - %2", info->text, info->title));
    match.setMultiLine(true);
    match.setSubtext(i18n("Copy text"));
    match.setIcon(QIcon::fromTheme(QStringLiteral("bible-icon")));

    match.setData(MatchAction::Copy);
    match.setId(i18n("%1_copy", url));
    matches->append(match);
}

// Informational match shown while no API key is configured: verse text cannot
// be copied without one, so instead of the copy match the runner offers this
// hint, and running it opens the runner configuration
void BibleRunner::makeSetKeyMatch(QList<KRunner::QueryMatch> *matches)
{
    KRunner::QueryMatch match(this);
    match.setText(i18n("Set the YouVersion API key to copy verse text"));
    match.setSubtext(i18n("Opens the Bible Runner configuration"));
    match.setIcon(QIcon::fromTheme(QStringLiteral("settings-configure")));
    // steady position below the browser match: Enter on the query should
    // keep doing the browsing
    match.setRelevance(0.5);
    match.setData(MatchAction::SetKey);
    matches->append(match);
}

void BibleRunner::run(const KRunner::RunnerContext & /*context*/, const KRunner::QueryMatch &match)
{
    switch (match.data().toInt())
    {
    case MatchAction::Browser:
        QProcess::startDetached(QLatin1String("xdg-open"), {match.urls().first().toString()});
        break;
    case MatchAction::SetKey:
        // As in the built-in help runner: systemsettings with the plasma search
        // module, whose page hosts the runners' configuration modules (our kcm
        // is installed in the kf6/krunner/kcms namespace and has no standalone
        // kcmshell id)
        {
            auto *const job = new KIO::CommandLauncherJob(QStringLiteral("systemsettings"),
                                                          {QStringLiteral("kcm_plasmasearch"),
                                                           QStringLiteral("--args"),
                                                           metadata().name()},
                                                          this);
            job->start();
        }
        break;
    default: // MatchAction::Copy
        QApplication::clipboard()->setText(match.text());
        break;
    }
}

K_PLUGIN_CLASS_WITH_JSON(BibleRunner, "biblerunner.json")

#include "biblerunner.moc"
