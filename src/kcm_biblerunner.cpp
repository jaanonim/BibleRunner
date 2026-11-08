// Created by jaanonim on 02.11.26.
//

#include "kcm_biblerunner.h"

#include <KLocalizedString>
#include <KPasswordLineEdit>
#include <KPluginFactory>

#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <vector>

// The YouVersion API layer (endpoint URLs, request submission, reply handling
// and the whole catalog fetch: the languages request and the paged bibles
// walk with their callback reporting) and the catalog cache file (source of
// the combo entries and target of successful fetches) live in the
// biblerunner.api and biblerunner.cache modules, which are shared with the
// runner plugin; the config file (krunnerrc) is handled by loadConfig()/
// saveConfig() in biblerunner.config. Only the fetch scheduling, the
// stale-generation bookkeeping and the UI stay local here.
import biblerunner.api;
import biblerunner.cache;
import biblerunner.config;

K_PLUGIN_CLASS(BibleRunnerConfig)

namespace
{
    // Fetches are debounced: the refresh cadence of the cached catalog (how
    // often it may be updated, and the rate-limit rationale) lives in the
    // biblerunner.cache module.
    constexpr int CATALOG_DEBOUNCE_MS = 700;
} // namespace

BibleRunnerConfig::BibleRunnerConfig(QObject *parent, const KPluginMetaData &data, const QVariantList & /*args*/)
    : KCModule(qobject_cast<QWidget *>(parent), data)
{
    // KCModule is not a QWidget itself; widget() returns the embedded widget
    QWidget *const widget = this->widget();
    auto *const layout = new QFormLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    appKeyEdit = new KPasswordLineEdit(widget);

    bibleIdCombo = new QComboBox(widget);
    bibleIdCombo->setEditable(true);
    bibleIdCombo->setInsertPolicy(QComboBox::NoInsert);

    network = new QNetworkAccessManager(this);

    // Fetch the version catalog from the YouVersion API once the user pauses
    // typing; startFetch() is a no-op without a key
    fetchDebounce = new QTimer(this);
    fetchDebounce->setSingleShot(true);
    fetchDebounce->setInterval(CATALOG_DEBOUNCE_MS);
    connect(fetchDebounce, &QTimer::timeout, this, &BibleRunnerConfig::startFetch);

    // Manual refetch of the version catalog: bypasses both the debounce timer
    // and the cache age. Usable only with an API key, so the enabled state
    // follows the key contents; load() and defaults() update it, too, because
    // their blocked passwordChanged does not reach the lambda below
    refreshButton = new QPushButton(widget);
    refreshButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    refreshButton->setAutoDefault(false);
    refreshButton->setToolTip(i18nc("@info:tooltip", "Refetch the bible version list from the YouVersion Platform API"));
    refreshButton->setEnabled(false);
    connect(refreshButton, &QPushButton::clicked, this, [this]()
            {
        fetchDebounce->stop();
        startFetch(); });

    connect(appKeyEdit, &KPasswordLineEdit::passwordChanged, this, [this]()
            {
        refreshButton->setEnabled(!appKeyEdit->password().isEmpty());
        scheduleFetch();
        markAsChanged(); });

    // Populate before connecting the combo's changed signal, so no spurious
    // markAsChanged() fires while the UI state is being established
    cache = readCatalogCache();
    applyCatalog();

    // The fetched version list is long (thousands of entries): let the user
    // type to filter it
    QCompleter *const completer = new QCompleter(bibleIdCombo->model(), bibleIdCombo);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    bibleIdCombo->setCompleter(completer);
    if (QLineEdit *const lineEdit = bibleIdCombo->lineEdit())
    {
        lineEdit->setClearButtonEnabled(true);
        connect(lineEdit, &QLineEdit::textEdited, this, [this]()
                { markAsChanged(); });
    }

    statusLabel = new QLabel(widget);
    statusLabel->setTextFormat(Qt::RichText);
    statusLabel->setWordWrap(true);

    auto const infoText = i18n("An API key can be acquired from <a href=\"%1\">YouVersion Platform</a> and is required to fetch verse content.",
                               QStringLiteral("https://platform.youversion.com/platform/apps"));
    auto *const infoLabel = new QLabel(infoText, widget);
    infoLabel->setTextFormat(Qt::RichText);
    infoLabel->setWordWrap(true);

    layout->addRow(i18nc("@label:textbox YouVersion Platform API key", "API key:"), appKeyEdit);
    // the refresh button sits next to the version list it refetches
    auto *const versionLayout = new QHBoxLayout;
    versionLayout->setContentsMargins(0, 0, 0, 0);
    versionLayout->addWidget(bibleIdCombo, 1);
    versionLayout->addWidget(refreshButton);
    layout->addRow(i18nc("@label:listbox bible version used by the runner", "Bible version:"), versionLayout);
    layout->addRow(statusLabel);
    layout->addRow(infoLabel);

    connect(bibleIdCombo, &QComboBox::currentIndexChanged, this, [this]()
            { markAsChanged(); });

    load();
}

void BibleRunnerConfig::scheduleFetch()
{
    // Reset the debounce timer, so one fetch runs after the last keystroke
    if (appKeyEdit->password().isEmpty())
    {
        return;
    }
    fetchDebounce->start();
}

void BibleRunnerConfig::startFetch()
{
    if (appKeyEdit->password().isEmpty())
    {
        return;
    }

    // Cancel any in-flight fetch: this generation replaces the catalog data
    // from scratch, and replies of earlier generations bail out early. The
    // fetch itself replaces cache.bibles, so the cached fetchedAt (age) is
    // left until a new fetch succeeds.
    const int generation = ++fetchGeneration;
    languagesDone = false;
    biblesDone = false;
    fetchError.clear();
    cache.bibles.clear();
    cache.languages = QJsonObject();

    statusLabel->setText(i18n("Fetching bible versions from the YouVersion API…"));

    // The api module runs both requests in parallel and reports each one once
    // its reply (for bibles: the last page) has been handled. cancelled stops
    // the paged bibles walk as soon as this generation is superseded; replies
    // of a stale generation are dropped by the generation check in the lambdas.
    const QString appKey = appKeyEdit->password();
    const auto cancelled = [this, generation]()
    {
        return generation != fetchGeneration;
    };

    fetchLanguages(network, appKey, this,
                   [this, generation](LanguagesResult result)
                   {
                       if (generation != fetchGeneration)
                       {
                           return;
                       }
                       cache.languages = std::move(result.languages);
                       if (!result.error.isEmpty())
                       {
                           fetchError = result.error;
                       }
                       languagesDone = true;
                       finishFetch(generation);
                   });

    fetchBibles(network, appKey, this, cancelled,
                [this, generation](BiblesResult result)
                {
                    if (generation != fetchGeneration)
                    {
                        return;
                    }
                    cache.bibles = std::move(result.bibles);
                    if (!result.error.isEmpty())
                    {
                        fetchError = result.error;
                    }
                    biblesDone = true;
                    finishFetch(generation);
                });
}

void BibleRunnerConfig::finishFetch(int generation)
{
    if (generation != fetchGeneration || !languagesDone || !biblesDone)
    {
        return;
    }

    if (fetchError.isEmpty() && !cache.bibles.isEmpty())
    {
        // the catalog belongs to this key: load()/save() skip re-fetching it
        fetchedKey = appKeyEdit->password();
        cache.fetchedAt = QDateTime::currentSecsSinceEpoch();
        writeCatalogCache(cache);
        applyCatalog();
        statusLabel->setText(i18np("%1 bible version loaded", "%1 bible versions loaded", cache.bibles.size()));
        return;
    }

    if (fetchError.isEmpty() && cache.bibles.isEmpty())
    {
        applyCatalog(); // clears the combo (HTTP 204: no versions for this key/ranges)
        statusLabel->setText(i18n("No bible versions available for this request."));
        return;
    }

    // The fetch failed. Keep what arrived: the bibles part may still be fresh
    // when only the languages request failed, and restore the cached entries
    // for everything else, so the list never shrinks below the cached one.
    const CatalogCache cached = readCatalogCache();

    if (cache.bibles.isEmpty())
    {
        cache.bibles = cached.bibles;
    }
    if (cache.languages.isEmpty())
    {
        cache.languages = cached.languages;
    }
    applyCatalog();
    statusLabel->setText(i18n("Bible version fetch failed: %1", fetchError));
}

void BibleRunnerConfig::applyCatalog()
{
    std::vector<std::pair<QString, QString>> versions;
    versions.reserve(cache.bibles.size());
    for (const BibleEntry &bible : std::as_const(cache.bibles))
    {
        // Same display format as the previously shipped versions.json list:
        // "language — title (abbreviation)"
        QString language = cache.languages.value(bible.languageTag).toString();
        if (language.isEmpty())
        {
            language = bible.languageTag;
        }
        QString display = language + QLatin1String(" - ") + bible.title;
        if (!bible.abbreviation.isEmpty())
        {
            display += QLatin1String(" (") + bible.abbreviation + QLatin1Char(')');
        }
        versions.emplace_back(display, bible.id);
    }

    std::sort(versions.begin(), versions.end());

    {
        const QSignalBlocker comboBlocker(bibleIdCombo);
        bibleIdCombo->clear();
        for (const auto &[display, id] : versions)
        {
            bibleIdCombo->addItem(display, id);
        }
    }

    reselectSaved();
}

void BibleRunnerConfig::reselectSaved()
{
    const QSignalBlocker comboBlocker(bibleIdCombo);
    const QString wanted = savedBibleId.isEmpty() ? QLatin1String(DEFAULT_BIBLE_ID) : savedBibleId;

    int index = bibleIdCombo->findData(wanted);
    if (index < 0 && !savedBibleId.isEmpty())
    {
        // The stored id is not in the fetched catalog (yet); keep it selectable
        // so that save() does not drop the setting while data is loading
        QString label = i18nc("@item:inlistbox version id not found in the version list",
                              "%1 (unknown version)", wanted);
        if (bibleIdCombo->findText(label) < 0)
        {
            bibleIdCombo->addItem(label, wanted);
        }
        index = bibleIdCombo->findData(wanted);
    }
    if (index >= 0)
    {
        bibleIdCombo->setCurrentIndex(index);
    }
}

void BibleRunnerConfig::load()
{
    const Config stored = loadConfig();
    savedBibleId = stored.bibleId;

    {
        const QSignalBlocker appKeyBlocker(appKeyEdit);
        appKeyEdit->setPassword(stored.appKey);
    }
    refreshButton->setEnabled(!stored.appKey.isEmpty());

    reselectSaved();

    // Refresh in the background only when the cached catalog is older than a
    // month (catalogCacheStale() also covers the no-cache-yet case); the
    // refresh button refetches at any time
    if (!stored.appKey.isEmpty() && catalogCacheStale(cache))
    {
        fetchDebounce->start(CATALOG_DEBOUNCE_MS);
    }

    setNeedsSave(false);
    setRepresentsDefaults(stored.appKey.isEmpty() && (savedBibleId.isEmpty() || savedBibleId == QLatin1String(DEFAULT_BIBLE_ID)));
}

void BibleRunnerConfig::save()
{
    // A fetched catalog entry stores the version id as item data; hand-typed
    // text has none. currentData() alone cannot tell them apart: the editable
    // line edit keeps the index of the previously selected item while the user
    // types (NoInsert inserts nothing), so its data would silently replace the
    // typed id with what was selected before (the default placeholder). The
    // data is therefore only used when the shown text is that item's text;
    // otherwise a non-empty hand-typed id is stored as-is, so it survives
    // reopening the settings instead of falling back to the default.
    const QString typed = bibleIdCombo->currentText().trimmed();
    const int index = bibleIdCombo->currentIndex();
    QVariant versionId;
    if (index >= 0 && bibleIdCombo->itemText(index).trimmed() == typed)
    {
        versionId = bibleIdCombo->itemData(index);
    }
    if (versionId.isValid())
    {
        savedBibleId = versionId.toString();
    }
    else if (!typed.isEmpty())
    {
        savedBibleId = typed;
    }

    Config current;
    current.appKey = appKeyEdit->password();
    current.bibleId = savedBibleId;
    saveConfig(current);

    // The typed key may not have been fetched yet (the debounce timer has not
    // run): refresh at once so the combo shows real entries instead of the
    // "(unknown version)" placeholder
    if (!appKeyEdit->password().isEmpty() && appKeyEdit->password() != fetchedKey)
    {
        fetchDebounce->stop();
        startFetch();
    }

    setNeedsSave(false);
    // same check as load(): the default state is an empty key and the default
    // version id (now also reachable by typing the default id by hand)
    setRepresentsDefaults(appKeyEdit->password().isEmpty() && savedBibleId == QLatin1String(DEFAULT_BIBLE_ID));
}

void BibleRunnerConfig::defaults()
{
    {
        const QSignalBlocker appKeyBlocker(appKeyEdit);
        appKeyEdit->setPassword(QString());
        const QSignalBlocker comboBlocker(bibleIdCombo);
        const int index = bibleIdCombo->findData(QLatin1String(DEFAULT_BIBLE_ID));
        if (index >= 0)
        {
            bibleIdCombo->setCurrentIndex(index);
        }
    }
    refreshButton->setEnabled(false);
    setNeedsSave(true);
    setRepresentsDefaults(true);
}

#include "kcm_biblerunner.moc"
