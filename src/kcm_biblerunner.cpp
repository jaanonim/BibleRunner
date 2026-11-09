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

import biblerunner.api;
import biblerunner.cache;
import biblerunner.config;

K_PLUGIN_CLASS(BibleRunnerConfig)

namespace
{
    constexpr int CATALOG_DEBOUNCE_MS = 700;
} // namespace

BibleRunnerConfig::BibleRunnerConfig(QObject *parent, const KPluginMetaData &data, const QVariantList & /*args*/)
    : KCModule(qobject_cast<QWidget *>(parent), data)
{
    // KCModule itself is no widget
    QWidget *const widget = this->widget();
    auto *const layout = new QFormLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    appKeyEdit = new KPasswordLineEdit(widget);

    bibleIdCombo = new QComboBox(widget);
    bibleIdCombo->setEditable(true);
    bibleIdCombo->setInsertPolicy(QComboBox::NoInsert);

    network = new QNetworkAccessManager(this);

    fetchDebounce = new QTimer(this);
    fetchDebounce->setSingleShot(true);
    fetchDebounce->setInterval(CATALOG_DEBOUNCE_MS);
    connect(fetchDebounce, &QTimer::timeout, this, &BibleRunnerConfig::startFetch);

    refreshButton = new QPushButton(widget); // bypasses debounce and cache age
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

    // populate before the signals fire, avoiding spurious markAsChanged()
    cache = readCatalogCache();
    applyCatalog();

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

    // supersede any in-flight fetch
    const int generation = ++fetchGeneration;
    languagesDone = false;
    biblesDone = false;
    fetchError.clear();
    cache.bibles.clear();
    cache.languages = QJsonObject();

    statusLabel->setText(i18n("Fetching bible versions from the YouVersion API…"));

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
        // catalog belongs to this key
        fetchedKey = appKeyEdit->password();
        cache.fetchedAt = QDateTime::currentSecsSinceEpoch();
        writeCatalogCache(cache);
        applyCatalog();
        statusLabel->setText(i18np("%1 bible version loaded", "%1 bible versions loaded", cache.bibles.size()));
        return;
    }

    if (fetchError.isEmpty() && cache.bibles.isEmpty())
    {
        applyCatalog(); // HTTP 204: nothing for this key
        statusLabel->setText(i18n("No bible versions available for this request."));
        return;
    }

    // on failure: keep what arrived, restore the rest from the cache
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
        // "language - title (abbreviation)"
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
        // stored id not in the fetched list (yet); keep it selectable
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

    if (!stored.appKey.isEmpty() && catalogCacheStale(cache))
    {
        fetchDebounce->start(CATALOG_DEBOUNCE_MS);
    }

    setNeedsSave(false);
    setRepresentsDefaults(stored.appKey.isEmpty() && (savedBibleId.isEmpty() || savedBibleId == QLatin1String(DEFAULT_BIBLE_ID)));
}

void BibleRunnerConfig::save()
{
    // itemData is only trusted when it matches the shown text: while the user
    // types, the line edit keeps the index (and data) of the previous item.
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

    // new key: fetch now instead of waiting for the debounce
    if (!appKeyEdit->password().isEmpty() && appKeyEdit->password() != fetchedKey)
    {
        fetchDebounce->stop();
        startFetch();
    }

    setNeedsSave(false);
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
