module;

#include <QString>

#include <KConfigGroup>
#include <KSharedConfig>

export module biblerunner.config;

export struct Config
{
    QString appKey;   // YouVersion Platform app key, sent as X-YVP-App-Key header
    QString bibleId;  // platform bible version id as string, e.g. "2095"

    bool isConfigured() const
    {
        return !appKey.isEmpty() && !bibleId.isEmpty();
    }
};

// Fallback default translation: used by the runner while no version is
// configured, and treated as the factory default by the configuration module.
export inline constexpr const char *DEFAULT_BIBLE_ID = "2095";

namespace
{
    // The config lives in the runner's KConfig group (krunnerrc /
    // Runners[biblerunner], the group returned by AbstractRunner::config()).
    // Everything that touches the config file — the group and entry names,
    // reading and writing — lives in this module; the runner and the
    // configuration module both call it and never open krunnerrc on their own.
    KConfigGroup runnerConfig()
    {
        // Same construction as AbstractRunner::config(), so both sides always
        // use Runners[biblerunner] in krunnerrc.
        KConfigGroup runners(KSharedConfig::openConfig(QStringLiteral("krunnerrc")), QStringLiteral("Runners"));
        return runners.group(QLatin1String("biblerunner"));
    }
} // namespace

export Config loadConfig()
{
    KConfigGroup group = runnerConfig();

    Config config;
    config.appKey = group.readEntry(QLatin1String("appKey"), QString());
    config.bibleId = group.readEntry(QLatin1String("bibleId"), QString());

    return config;
}

// Write the configuration so that KRunner picks the change up live: the
// KConfig::Notify flag makes KConfig broadcast it on the session bus, which
// makes the KConfigWatcher in RunnerManager call reloadConfiguration() on the
// running plugin.
export void saveConfig(const Config &config)
{
    KConfigGroup group = runnerConfig();

    group.writeEntry(QLatin1String("appKey"), config.appKey, KConfigBase::Notify);
    group.writeEntry(QLatin1String("bibleId"), config.bibleId, KConfigBase::Notify);

    // KConfig only performs the file write and the broadcast on sync()
    group.sync();
}
