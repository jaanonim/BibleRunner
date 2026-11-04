// Created by jaanonim on 02.11.26.
//

#include "kcm_biblerunner.h"

#include <KLocalizedString>
#include <KPasswordLineEdit>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QCompleter>
#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <vector>

K_PLUGIN_CLASS(BibleRunnerConfig)

// Fallback default translation, mirroring biblerunner.cpp
static const char DEFAULT_BIBLE_ID[] = "2095";

BibleRunnerConfig::BibleRunnerConfig(QObject *parent, const KPluginMetaData &data, const QVariantList & /*args*/)
    : KCModule(qobject_cast<QWidget *>(parent), data)
{
    // Same config group as AbstractRunner::config() in the runner, so both sides
    // read/write Runners[biblerunner] in krunnerrc.
    KConfigGroup runners(KSharedConfig::openConfig(QStringLiteral("krunnerrc")), QStringLiteral("Runners"));
    config = runners.group(QStringLiteral("biblerunner"));

    // KCModule is not a QWidget itself; widget() returns the embedded widget
    QWidget *const widget = this->widget();
    auto *const layout = new QFormLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    appKeyEdit = new KPasswordLineEdit(widget);

    bibleIdCombo = new QComboBox(widget);
    bibleIdCombo->setEditable(true);
    bibleIdCombo->setInsertPolicy(QComboBox::NoInsert);
    // Populate before connecting the changed signals, so no spurious markAsChanged()
    populateVersions();

    // The shipped version list is long (thousands of entries): let the user type to filter it
    QCompleter *const completer = new QCompleter(bibleIdCombo->model(), bibleIdCombo);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    bibleIdCombo->setCompleter(completer);
    if (QLineEdit *const lineEdit = bibleIdCombo->lineEdit())
    {
        lineEdit->setClearButtonEnabled(true);
    }

    auto *const infoLabel = new QLabel(
        i18n("An API key from <a href=\"%1\">YouVersion Platform</a> is needed to copy verse text.",
             QStringLiteral("https://platform.bible")),
        widget);
    infoLabel->setTextFormat(Qt::RichText);
    infoLabel->setWordWrap(true);

    layout->addRow(i18nc("@label:textbox YouVersion Platform API key", "API key:"), appKeyEdit);
    layout->addRow(i18nc("@label:listbox bible version used by the runner", "Bible version:"), bibleIdCombo);
    layout->addRow(infoLabel);

    connect(appKeyEdit, &KPasswordLineEdit::passwordChanged, this, [this]() { markAsChanged(); });
    connect(bibleIdCombo, &QComboBox::currentIndexChanged, this, [this]() { markAsChanged(); });
    if (QLineEdit *const lineEdit = bibleIdCombo->lineEdit())
    {
        connect(lineEdit, &QLineEdit::textEdited, this, [this]() { markAsChanged(); });
    }

    load();
}

void BibleRunnerConfig::populateVersions()
{
    QFile file(QStringLiteral(BIBLERUNNER_VERSIONS_FILE));
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "BibleRunner kcm: could not open bible version list" << file.fileName();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "BibleRunner kcm: failed to parse bible version list" << file.fileName() << ":"
                   << parseError.errorString();
        return;
    }

    // versions.json is a map of language code -> {name, data: [{id, name, abbreviation}]}
    std::vector<std::pair<QString, QString>> versions;
    const QJsonObject json = doc.object();
    for (const QString &langCode : json.keys())
    {
        const QJsonObject lang = json.value(langCode).toObject();
        const QString language = lang.value(QLatin1String("name")).toString();
        const QJsonArray entries = lang.value(QLatin1String("data")).toArray();
        for (const QJsonValue &value : entries)
        {
            const QJsonObject version = value.toObject();
            const QString id = QString::number(version.value(QLatin1String("id")).toVariant().toLongLong());
            QString display =
                language + QLatin1String(" — ") + version.value(QLatin1String("name")).toString();
            const QString abbreviation = version.value(QLatin1String("abbreviation")).toString();
            if (!abbreviation.isEmpty())
            {
                display += QLatin1String(" (") + abbreviation + QLatin1Char(')');
            }
            versions.emplace_back(display, id);
        }
    }

    std::sort(versions.begin(), versions.end());
    for (const auto &[display, id] : versions)
    {
        bibleIdCombo->addItem(display, id);
    }
    if (bibleIdCombo->count() == 0)
    {
        qWarning() << "BibleRunner kcm: no bible versions found in" << file.fileName();
    }
}

void BibleRunnerConfig::load()
{
    const QString appKey = config.readEntry(QStringLiteral("appKey"), QString());
    const QString bibleId = config.readEntry(QStringLiteral("bibleId"), QString());

    {
        const QSignalBlocker appKeyBlocker(appKeyEdit);
        appKeyEdit->setPassword(appKey);
    }

    {
        const QSignalBlocker comboBlocker(bibleIdCombo);
        int index = bibleIdCombo->findData(bibleId);
        if (index < 0)
        {
            if (bibleId.isEmpty())
            {
                index = bibleIdCombo->findData(QLatin1String(DEFAULT_BIBLE_ID));
            }
            else
            {
                // The stored id is not in the shipped list anymore; keep it selectable
                bibleIdCombo->addItem(i18nc("@item:inlistbox version id not found in the version list",
                                            "%1 (unknown version)", bibleId), bibleId);
                index = bibleIdCombo->count() - 1;
            }
        }
        if (index >= 0)
        {
            bibleIdCombo->setCurrentIndex(index);
        }
    }

    setNeedsSave(false);
    setRepresentsDefaults(appKey.isEmpty()
                          && (bibleId.isEmpty() || bibleId == QLatin1String(DEFAULT_BIBLE_ID)));
}

void BibleRunnerConfig::save()
{
    config.writeEntry(QStringLiteral("appKey"), appKeyEdit->password(), KConfigBase::Notify);

    // Only write a selected version; free-typed text without a completion has no id data
    const QVariant versionId = bibleIdCombo->currentData();
    if (versionId.isValid())
    {
        config.writeEntry(QStringLiteral("bibleId"), versionId.toString(), KConfigBase::Notify);
    }

    // The KConfig::Notify flag makes KConfig broadcast the change on the session bus,
    // which makes the KConfigWatcher in RunnerManager call reloadConfiguration()
    config.sync();

    setNeedsSave(false);
    setRepresentsDefaults(appKeyEdit->password().isEmpty()
                          && versionId == QVariant(QLatin1String(DEFAULT_BIBLE_ID)));
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

    setNeedsSave(true);
    setRepresentsDefaults(true);
}

#include "kcm_biblerunner.moc"
