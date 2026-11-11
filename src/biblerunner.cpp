#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include "biblerunner.h"
#include <QIcon>
#include <QProcess>
#include <QApplication>
#include <QClipboard>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
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

    addSyntax(QStringLiteral("<book> <chapter>"), i18n("Open a whole chapter, e.g. Psalm 119"));
    addSyntax(QStringLiteral("<book> <chapter>:<verse>"), i18n("Open a verse, e.g. John 3:16"));
    addSyntax(QStringLiteral("<book> <chapter>:<verse>-<last verse>"), i18n("Open a verse range, e.g. 1kor 13:4-7"));
}

void BibleRunner::match(KRunner::RunnerContext &context)
{
    Config loaded = loadConfigGuarded();

    QString bibleId = QLatin1String(DEFAULT_BIBLE_ID);
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

    match.setData(static_cast<int>(MatchAction::Browser));
    match.setId(QStringLiteral("%1_browser").arg(url)); // ids are not translated
    matches->append(match);
}

void BibleRunner::makeCopyMatch(QList<KRunner::QueryMatch> *matches, const QString &passage, const QString &url)
{
    // guarded copy: reloadConfiguration() may swap the config mid-request
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

    match.setData(static_cast<int>(MatchAction::Copy));
    match.setId(QStringLiteral("%1_copy").arg(url));
    matches->append(match);
}

void BibleRunner::makeSetKeyMatch(QList<KRunner::QueryMatch> *matches)
{
    KRunner::QueryMatch match(this);
    match.setText(i18n("Set the YouVersion API key to copy verse text"));
    match.setSubtext(i18n("Opens the Bible Runner configuration"));
    match.setIcon(QIcon::fromTheme(QStringLiteral("settings-configure")));
    match.setRelevance(0.5); // below the browser match, so Enter keeps browsing
    match.setData(static_cast<int>(MatchAction::SetKey));
    matches->append(match);
}

void BibleRunner::run(const KRunner::RunnerContext & /*context*/, const KRunner::QueryMatch &match)
{
    switch (static_cast<MatchAction>(match.data().toInt()))
    {
    case MatchAction::Browser:
        QProcess::startDetached(QLatin1String("xdg-open"), {match.urls().first().toString()});
        break;
    case MatchAction::SetKey:
        // as in the built-in help runner: no standalone kcmshell for this kcm
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
