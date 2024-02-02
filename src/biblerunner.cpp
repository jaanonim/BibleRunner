//
// Created by jaanonim on 01.02.24.
//

#include <KLocalizedString>
#include <KNotificationJobUiDelegate>
#include "biblerunner.h"
#include "data/books.h"
#include <KIO/OpenUrlJob>
#include <QProcess>
#include <QApplication>
#include <QClipboard>

const QRegularExpression linkRegex(QLatin1String(R"(([123]\s?)?\p{L}+\s?\d{1,3}([:,.]\s?\d{1,3}(-\d{1,3})?)?)"));
const QRegularExpression bookRegex(QLatin1String(R"(([123]\s?)?\p{L}+)"));
const QRegularExpression separatorRegex(QLatin1String(R"([-:,.]+)"));
const QRegularExpression spaceRegex(QLatin1String("\\s"));

BibleRunner::BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
        : AbstractRunner(parent, data, args) {
    setPriority(LowPriority);
}

void BibleRunner::init() {
    reloadConfiguration();
    connect(this, &Plasma::AbstractRunner::prepare, this, []() {
    });
    connect(this, &Plasma::AbstractRunner::teardown, this, []() {
        // Cleanup data from the match session. This gets called from the main thread
    });
}

void BibleRunner::match(Plasma::RunnerContext &context) {
    QString query = context.query();

    if (query.startsWith(QLatin1Char('?'))) {
        return;
    }

    query = query.trimmed();

    QRegularExpressionMatch re_match = linkRegex.match(query);
    if (!re_match.hasMatch()) { return; }

    QString text = re_match.captured(0).trimmed();
    if (text.length() != query.length()) { return; }

    re_match = bookRegex.match(text);
    if (!re_match.hasMatch()) { return; }

    QString inBook = re_match.captured(0);
    text.remove(0, inBook.length());
    QString book = inBook.toLower().trimmed().remove(spaceRegex);
    if (auto search = BOOKS.find(book.toStdString()); search != BOOKS.end()) {
        book = QString::fromStdString(search->second);
    }

    text = text.trimmed();
    QStringList split = text.split(separatorRegex);
    QString url = getUrl(book);
    if (split.length() == 1) {
        text = split[0];
        url.append(split[0]);
    } else if (split.length() == 2) {
        text = split[0] + QLatin1Char(':') + split[1];
        url.append(split[0] + QLatin1Char('.') + split[1]);
    } else if (split.length() == 3) {
        text = split[0] + QLatin1Char(':') + split[1] + QLatin1Char('-') + split[2];
        url.append(split[0] + QLatin1Char('.') + split[1] + QLatin1Char('-') + split[2]);
    } else {
        return;
    }

    QList<Plasma::QueryMatch> matches;
    Plasma::QueryMatch match(this);
    match.setText(i18n("%1 %2", book, text));
    match.setSubtext(i18n("Open Bible in browser"));
    match.setIcon(icon());

    QUrl qurl;
    qurl.setUrl(url);
    QList<QUrl> qulr_list;
    qulr_list.append(qurl);
    match.setUrls(qulr_list);

    match.setData(1);
    match.setId(i18n("%1_browser", url));
    matches.append(match);

    context.addMatches(matches);
}

QString BibleRunner::getUrl(QString book) {
    QString base = QLatin1String("https://www.bible.com/bible");
    return i18n("%1/%2/%3.", base, QLatin1String("2095"), book);
}

void BibleRunner::run(const Plasma::RunnerContext & /*context*/, const Plasma::QueryMatch &match) {
    // KIO::OpenUrlJob autodeletes itself, so we can just create it and forget it!

    if (match.data() == 1) {
        QProcess::startDetached(QLatin1String("x-www-browser"), {match.urls().first().toString()});
    } else {
        QApplication::clipboard()->setText(match.text());
    }

//    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(match.data().toString()));
//    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
//    job->setRunExecutables(false);
//    job->start();
}


K_PLUGIN_CLASS_WITH_JSON(BibleRunner, "biblerunner.json")

#include "biblerunner.moc"