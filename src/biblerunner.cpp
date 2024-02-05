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
#include <QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore>
#include <QObject>

const QRegularExpression linkRegex(QLatin1String(R"(([123]\s?)?\p{L}+\s?\d{1,3}([:,.]\s?\d{1,3}(-\d{1,3})?)?)"));
const QRegularExpression bookRegex(QLatin1String(R"(([123]\s?)?\p{L}+)"));
const QRegularExpression separatorRegex(QLatin1String(R"([-:,.]+)"));
const QRegularExpression spaceRegex(QLatin1String("\\s"));
const QRegularExpression htmlRegex(QLatin1String(
        R"(<script\s*id="__NEXT_DATA__"\s*type="application\/json"\s*>.+?(?=<\/script>\s*<\/body>\s*<\/html>))"));
const QRegularExpression htmlDelRegex(QLatin1String(
        R"(<script\s*id="__NEXT_DATA__"\s*type="application\/json"\s*>)"));

BibleRunner::BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
        : AbstractRunner(parent, data, args) {
    setPriority(LowPriority);
}

void BibleRunner::init() {
    reloadConfiguration();
    connect(this, &Plasma::AbstractRunner::prepare, this, []() {});
    connect(this, &Plasma::AbstractRunner::teardown, this, []() {});
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
    } else {
        return;
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
    makeBrowserMatch(&matches, book, text, url);
    makeCopyMatch(&matches, url);
    context.addMatches(matches);
}

void BibleRunner::makeBrowserMatch(QList<Plasma::QueryMatch> *matches, const QString &book, const QString &text,
                                   const QString &url) {
    Plasma::QueryMatch match(this);
    match.setText(i18n("Open %1 %2", book, text));
    match.setSubtext(i18n("Open Bible in browser"));
    match.setIcon(icon());

    QUrl qurl;
    qurl.setUrl(url);
    QList<QUrl> qulr_list;
    qulr_list.append(qurl);
    match.setUrls(qulr_list);

    match.setData(1);
    match.setId(i18n("%1_browser", url));
    matches->append(match);
}

void BibleRunner::makeCopyMatch(QList<Plasma::QueryMatch> *matches, const QString &url) {

    QString data = getTextOfVerse(url);

    if (data.length() < 1) { return; }

    QRegularExpressionMatch re_match = htmlRegex.match(data);
    if (!re_match.hasMatch()) { return; }

    QString json_str = re_match.captured(0);
    json_str.remove(htmlDelRegex);

    QJsonObject json = QJsonDocument::fromJson(json_str.toUtf8()).object();

    QJsonObject stuff = json.value(QLatin1String("props")).toObject().value(QLatin1String("pageProps")).toObject();

    if (stuff.value(QLatin1String("type")).toVariant().toString() != QLatin1String("verse")) { return; }

    QString title = stuff.value(QLatin1String("referenceTitle")).toObject().value(
            QString(QLatin1String("title"))).toVariant().toString();
    QString abbriv = stuff.value(QLatin1String("version")).toObject().value(
            QString(QLatin1String("local_abbreviation"))).toVariant().toString();


    QJsonArray verses_array = stuff.value(QLatin1String("verses")).toArray();
    QString verses = QString::fromUtf8("");
    for (auto verse: verses_array) {
        verses += verse.toObject().value(QLatin1String("content")).toVariant().toString();
        verses += QString::fromUtf8(" ");
    }
    verses = verses.trimmed();

    Plasma::QueryMatch match(this);
    match.setText(i18n("%1 - %2 %3", verses, title, abbriv));
    match.setMultiLine(true);
    match.setSubtext(i18n("Copy text"));
    match.setIcon(icon());

    match.setData(0);
    match.setId(i18n("%1_copy", url));
    matches->append(match);
}

QString BibleRunner::getTextOfVerse(const QString &url) {
    QUrl qurl(url);

    // create custom temporary event loop on stack
    QEventLoop eventLoop;

    // "quit()" the event-loop, when the network request "finished()"
    QNetworkAccessManager mgr;
    QObject::connect(&mgr, SIGNAL(finished(QNetworkReply * )), &eventLoop, SLOT(quit()));

    QNetworkRequest req(qurl);
    QNetworkReply *reply = mgr.get(req);
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        delete reply;
        return QString::fromUtf8(data);
    } else {
        delete reply;
        return QLatin1String("");
    }
}

QString BibleRunner::getUrl(const QString &book) {
    QString base = QLatin1String("https://www.bible.com/bible");
    return i18n("%1/%2/%3.", base, QLatin1String("2095"), book);
}

void BibleRunner::run(const Plasma::RunnerContext & /*context*/, const Plasma::QueryMatch &match) {
    if (match.data() == 1) {
        QProcess::startDetached(QLatin1String("x-www-browser"), {match.urls().first().toString()});
    } else {
        QApplication::clipboard()->setText(match.text());
    }
}


K_PLUGIN_CLASS_WITH_JSON(BibleRunner, "biblerunner.json")

#include "biblerunner.moc"