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

import biblerunner.data.books;
import biblerunner.query;

BibleRunner::BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList & /*args*/)
    : AbstractRunner(parent, data) {}

void BibleRunner::reloadConfiguration()
{
}

void BibleRunner::init()
{
    reloadConfiguration();
    connect(this, &KRunner::AbstractRunner::prepare, this, []() {});
    connect(this, &KRunner::AbstractRunner::teardown, this, []() {});
}

void BibleRunner::match(KRunner::RunnerContext &context)
{
    auto info = parseQuery(context.query());
    if (!info)
    {
        return;
    }

    QList<KRunner::QueryMatch> matches;
    makeBrowserMatch(&matches, info->book, info->text, info->url.toString());
    makeCopyMatch(&matches, info->url.toString());
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

void BibleRunner::makeCopyMatch(QList<KRunner::QueryMatch> *matches, const QString &url)
{
    auto info = fetchVerse(QUrl(url));
    if (!info)
    {
        return;
    }

    KRunner::QueryMatch match(this);
    match.setText(i18n("%1 - %2 %3", info->text, info->title, info->abbreviation));
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
    {
        QProcess::startDetached(QLatin1String("x-www-browser"), {match.urls().first().toString()});
    }
    else
    {
        QApplication::clipboard()->setText(match.text());
    }
}

K_PLUGIN_CLASS_WITH_JSON(BibleRunner, "biblerunner.json")

#include "biblerunner.moc"
