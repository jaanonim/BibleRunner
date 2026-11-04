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

// The config lives in the runner's KConfig group (krunnerrc / Runners[biblerunner],
// the group returned by AbstractRunner::config()). The configuration module
// (kcm_biblerunner) writes and reads the same group, and KRunner's KConfigWatcher
// calls reloadConfiguration() automatically when it changes.
export Config loadConfig()
{
    // Same construction as AbstractRunner::config(), so both sides always use
    // Runners[biblerunner] in krunnerrc.
    KConfigGroup runners(KSharedConfig::openConfig(QStringLiteral("krunnerrc")), QStringLiteral("Runners"));
    KConfigGroup group = runners.group(QLatin1String("biblerunner"));

    Config config;
    config.appKey = group.readEntry(QLatin1String("appKey"), QString());
    config.bibleId = group.readEntry(QLatin1String("bibleId"), QString());

    return config;
}
