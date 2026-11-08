#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

// The bible version catalog data model, defined here outside the
// biblerunner.cache module for the same reason as lookupBook in books.h:
// kcm_biblerunner is a plain translation unit whose Q_OBJECT class is
// compiled by the moc in a TU that does not import modules, so no type
// declared inside a module can appear in kcm_biblerunner.h. Plain Qt types
// in this header are visible to everyone: the biblerunner.cache module
// includes it in its global module fragment (see modules/cache.cppm), the
// biblerunner.api module includes it too (its paged bibles walk fills
// BibleEntry vectors into BiblesResult), and the configuration module
// includes it wherever the catalog is used.

struct BibleEntry
{
    QString id;          // version id as string, e.g. "3034"
    QString title;
    QString abbreviation;
    QString languageTag; // BCP 47 canonical tag of the version's language
};

// The version catalog as held in memory and cached on disk: the display
// names of the languages and the bible versions fetched from the
// YouVersion Platform API.
struct CatalogCache
{
    QJsonObject languages; // language id (BCP 47) -> display name
    QVector<BibleEntry> bibles;
    qint64 fetchedAt = 0;  // seconds since epoch of the last successful fetch
};
