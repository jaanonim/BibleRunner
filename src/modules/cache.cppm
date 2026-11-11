module;

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QVector>

#include "catalog.h"

export module biblerunner.cache;

namespace
{
    constexpr qint64 CATALOG_CACHE_MAX_AGE_SECS = 30 * 24 * 3600; // a month

    QString catalogCacheDir()
    {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QLatin1String("/biblerunner");
    }

    QString catalogCachePath()
    {
        return catalogCacheDir() + QLatin1String("/catalog.json");
    }
} // namespace

export CatalogCache readCatalogCache()
{
    CatalogCache cache;

    QFile file(catalogCachePath());
    if (!file.open(QIODevice::ReadOnly))
        return cache;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return cache;

    const QJsonObject root = doc.object();
    const QJsonArray cachedBibles = root.value(QLatin1String("bibles")).toArray();
    if (cachedBibles.isEmpty())
        return cache;

    cache.languages = root.value(QLatin1String("languages")).toObject();
    for (const QJsonValue &value : cachedBibles)
    {
        const QJsonObject version = value.toObject();
        BibleEntry entry;
        entry.id = version.value(QLatin1String("id")).toVariant().toString();
        entry.title = version.value(QLatin1String("title")).toString();
        entry.abbreviation = version.value(QLatin1String("abbreviation")).toString();
        entry.languageTag = version.value(QLatin1String("language_tag")).toString();
        if (!entry.id.isEmpty())
            cache.bibles.append(entry);
    }
    cache.fetchedAt = root.value(QLatin1String("fetched_at")).toVariant().toLongLong();
    return cache;
}

export void writeCatalogCache(const CatalogCache &cache)
{
    QDir().mkpath(catalogCacheDir());

    QJsonArray bibles;
    for (const BibleEntry &entry : cache.bibles)
    {
        QJsonObject version;
        version.insert(QLatin1String("id"), entry.id);
        version.insert(QLatin1String("title"), entry.title);
        version.insert(QLatin1String("abbreviation"), entry.abbreviation);
        version.insert(QLatin1String("language_tag"), entry.languageTag);
        bibles.append(version);
    }

    QJsonObject root;
    root.insert(QLatin1String("fetched_at"), double(cache.fetchedAt));
    root.insert(QLatin1String("languages"), cache.languages);
    root.insert(QLatin1String("bibles"), bibles);

    // atomic replace
    QSaveFile file(catalogCachePath());
    if (!file.open(QIODevice::WriteOnly))
        return;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

export bool catalogCacheStale(const CatalogCache &cache)
{
    if (cache.bibles.isEmpty() || cache.fetchedAt <= 0)
        return true;

    const qint64 age = QDateTime::currentSecsSinceEpoch() - cache.fetchedAt;
    return age > CATALOG_CACHE_MAX_AGE_SECS;
}
