module;

#include <optional>

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

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

export std::optional<Config> loadConfig()
{
    QString configDir = qEnvironmentVariable("BIBLERUNNER_CONFIG_PATH");
    if (configDir.isEmpty())
    {
        configDir = qEnvironmentVariable("XDG_CONFIG_HOME");
    }
    if (configDir.isEmpty())
    {
        QString home = qEnvironmentVariable("HOME");
        if (home.isEmpty())
        {
            return std::nullopt;
        }
        configDir = home + QLatin1String("/.config");
    }
    if (!configDir.endsWith(QLatin1Char('/')))
    {
        configDir += QLatin1Char('/');
    }

    QString configPath = configDir + QLatin1String("biblerunner.json");

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        qWarning() << "BibleRunner: failed to parse config file" << configPath << ":" << parseError.errorString();
        return std::nullopt;
    }
    if (!doc.isObject())
    {
        qWarning() << "BibleRunner: config file is not a JSON object:" << configPath;
        return std::nullopt;
    }

    QJsonObject json = doc.object();

    Config config;
    config.appKey = json.value(QLatin1String("app_key")).toString();
    config.bibleId = json.value(QLatin1String("bible_id")).toVariant().toString();

    return config;
}
