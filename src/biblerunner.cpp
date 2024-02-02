//
// Created by jaanonim on 01.02.24.
//

#include <QDir>
#include <KLocalizedString>
#include "biblerunner.h"

BibleRunner::BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
        : AbstractRunner(parent, data, args) {
    setPriority(LowPriority);
}

void BibleRunner::init() {
    reloadConfiguration();
    connect(this, &Plasma::AbstractRunner::prepare, this, []() {
        // Initialize data for the match session. This gets called from the main thread
    });
    connect(this, &Plasma::AbstractRunner::teardown, this, []() {
        // Cleanup data from the match session. This gets called from the main thread
    });
}

void BibleRunner::match(Plasma::RunnerContext &context) {
    QString query = context.query();
    if (query == QLatin1Char('.') || query == QLatin1String("..")) {
        return;
    }
    // This should not get in the way of the Help-Runner which gets triggered by queries starting with '?'
    if (query.startsWith(QLatin1Char('?'))) {
        return;
    }

    if (!m_triggerWord.isEmpty()) {
        if (!query.startsWith(m_triggerWord)) {
            return;
        }

        query.remove(0, m_triggerWord.length());
    }

    if (query.length() > 2) {
        query.prepend(QLatin1Char('*')).append(QLatin1Char('*'));
    }

    QList<Plasma::QueryMatch> matches;
    Plasma::QueryMatch match(this);
    match.setText(i18n("lol"));
    match.setData(i18n("grfd e yfdh fg "));
    match.setId(i18n("lol"));
    matches.append(match);

    context.addMatches(matches);
}

void BibleRunner::run(const Plasma::RunnerContext & /*context*/, const Plasma::QueryMatch &match) {
    // KIO::OpenUrlJob autodeletes itself, so we can just create it and forget it!
//    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(match.data().toString()));
//    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
//    job->setRunExecutables(false);
//    job->start();
}


K_PLUGIN_CLASS_WITH_JSON(BibleRunner, "biblerunner.json")

#include "biblerunner.moc"