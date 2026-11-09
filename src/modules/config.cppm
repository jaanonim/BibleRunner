module;

#include <QString>

#include <KConfigGroup>
#include <KSharedConfig>

export module biblerunner.config;

export struct Config
{
    QString appKey;  // YouVersion Platform app key
    QString bibleId; // version id, e.g. "2095"

    bool isConfigured() const
    {
        return !appKey.isEmpty() && !bibleId.isEmpty();
    }
};

export inline constexpr const char *DEFAULT_BIBLE_ID = "2095";

namespace
{
    // krunnerrc / Runners[biblerunner], as AbstractRunner::config() builds it
    KConfigGroup runnerConfig()
    {
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

// Notify: the running KRunner reloads the plugin live
export void saveConfig(const Config &config)
{
    KConfigGroup group = runnerConfig();

    group.writeEntry(QLatin1String("appKey"), config.appKey, KConfigBase::Notify);
    group.writeEntry(QLatin1String("bibleId"), config.bibleId, KConfigBase::Notify);

    // nothing written/broadcast until sync()
    group.sync();
}
