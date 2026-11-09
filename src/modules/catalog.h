#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

// plain header, no modules: the moc TU of kcm_biblerunner cannot import them
struct BibleEntry
{
    QString id;          // version id as string, e.g. "3034"
    QString title;
    QString abbreviation;
    QString languageTag; // BCP 47 tag of the version's language
};

struct CatalogCache
{
    QJsonObject languages; // language id -> display name
    QVector<BibleEntry> bibles;
    qint64 fetchedAt = 0; // secs since epoch of the last successful fetch
};
