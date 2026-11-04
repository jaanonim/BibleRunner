//
// Created by jaanonim on 01.02.24.
//

#include <KLocalizedString>
#include "biblerunner.h"
#include <QIcon>
#include <QProcess>
#include <QApplication>
#include <QClipboard>
#include <QtCore>
#include <QObject>
#include <mutex>

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

    QString bibleId = QLatin1String("2095"); // fallback default translation
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
    {
        makeCopyMatch(&matches, info->passage, info->url.toString());
    }
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

    match.setData(1);
    match.setId(i18n("%1_browser", url));
    matches->append(match);
}

void BibleRunner::makeCopyMatch(QList<KRunner::QueryMatch> *matches, const QString &passage, const QString &url)
{
    // Only called with a configured runner (gated in match()); the guarded copy is
    // taken here so a concurrent reloadConfiguration() cannot tear down the Config
    // while the network request uses it.
    auto info = fetchVerse(loadConfigGuarded(), passage);
    if (!info)
    {
        return;
    }

    KRunner::QueryMatch match(this);
    match.setText(i18n("%1 - %2", info->text, info->title));
    match.setMultiLine(true);
    match.setSubtext(i18n("Copy text"));
    match.setIcon(QIcon::fromTheme(QStringLiteral("bible-icon")));

    match.setData(0);
    match.setId(i18n("%1_copy", url));
    matches->append(match);
}

void BibleRunner::run(const KRunner::RunnerContext & /*context*/, const KRunner::QueryMatch &match)
{
    if (match.data() == 1)
        QProcess::startDetached(QLatin1String("xdg-open"), {match.urls().first().toString()});
    else
        QApplication::clipboard()->setText(match.text());
}

K_PLUGIN_CLASS_WITH_JSON(BibleRunner, "biblerunner.json")

#include "biblerunner.moc"
