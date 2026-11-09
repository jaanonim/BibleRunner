#pragma once

#include <KCModule>

#include "modules/catalog.h"

class KPasswordLineEdit;
class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QPushButton;
class QTimer;

/// Configuration module of the "Bible Runner" KRunner plugin.
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
    // rebuild the combo from the cached catalog
    void applyCatalog();
    void reselectSaved();

    void scheduleFetch();
    void startFetch();
    void finishFetch(int generation);

    KPasswordLineEdit *appKeyEdit = nullptr;
    QComboBox *bibleIdCombo = nullptr;
    QLabel *statusLabel = nullptr;
    QNetworkAccessManager *network = nullptr;
    QTimer *fetchDebounce = nullptr;
    QPushButton *refreshButton = nullptr; // manual catalog refetch

    QString savedBibleId;
    QString fetchedKey; // key the current combo data was fetched with

    // in-memory catalog, synced with the cache file
    CatalogCache cache;

    int fetchGeneration = 0;
    bool languagesDone = true;
    bool biblesDone = true;
    QString fetchError;
};
