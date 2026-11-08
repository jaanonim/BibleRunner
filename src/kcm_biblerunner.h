// Created by jaanonim on 02.11.26.
//

#pragma once

#include <KCModule>

#include "modules/catalog.h"

class KPasswordLineEdit;
class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QPushButton;
class QTimer;

/**
 * Configuration module for the "Bible Runner" KRunner plugin.
 *
 * All config file access (krunnerrc / Runners[biblerunner], entries written
 * with the KConfig::Notify flag so KRunner's KConfigWatcher picks the change
 * up and calls reloadConfiguration()) lives in the biblerunner.config module;
 * see loadConfig()/saveConfig() there.
 *
 * The bible version list is not shipped with the plugin; it is fetched live
 * from the YouVersion Platform API using the app key entered here. The fetch
 * itself (one languages request plus one request per bibles page, followed by
 * the pagination protocol) is run by the biblerunner.api module and reports
 * through callbacks. Loading and saving of the cache file is done by the
 * biblerunner.cache module; the file lives under QStandardPaths::CacheLocation,
 * so the module opens instantly and still shows a list when offline. It is
 * refetched automatically only when it is older than a month; a refresh
 * button refetches it at any time.
 */
class BibleRunnerConfig : public KCModule
{
    Q_OBJECT

public:
    BibleRunnerConfig(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

public:
    void load() override;
    void save() override;
    void defaults() override;

private:
    // combo state: rebuild the item list from cache.bibles/cache.languages and
    // make sure the stored bibleId stays selectable
    void applyCatalog();
    void reselectSaved();

    // YouVersion API catalog fetch: scheduleFetch()/startFetch() start the
    // fetchLanguages()/fetchBibles() requests of the biblerunner.api module
    // (one languages request plus one request per bibles page) for the current
    // generation, and finishFetch() applies the reported results
    void scheduleFetch();
    void startFetch();
    void finishFetch(int generation);

    KPasswordLineEdit *appKeyEdit = nullptr;
    QComboBox *bibleIdCombo = nullptr;
    QLabel *statusLabel = nullptr;
    QNetworkAccessManager *network = nullptr;
    QTimer *fetchDebounce = nullptr;
    QPushButton *refreshButton = nullptr; // manual catalog refetch, enabled only with an API key

    QString savedBibleId;
    QString fetchedKey; // app key the current combo data was fetched with

    // latest catalog data (filled from the cache by readCatalogCache(), then
    // replaced by fetches and written back with writeCatalogCache(); the
    // handling of the catalog file is in modules/cache.cppm)
    CatalogCache cache;

    // fetch state: both steps must complete before the combo is rebuilt
    int fetchGeneration = 0;
    bool languagesDone = true;
    bool biblesDone = true;
    QString fetchError;
};
