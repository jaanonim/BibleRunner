// Created by jaanonim on 02.11.26.
//

#pragma once

#include <KCModule>
#include <KConfigGroup>

class KPasswordLineEdit;
class QComboBox;

/**
 * Configuration module for the "Bible Runner" KRunner plugin.
 *
 * The settings are stored in the runner's own config group of "krunnerrc"
 * (Runners[biblerunner], the group returned by AbstractRunner::config()).
 * Entries are written with the KConfig::Notify flag, so KRunner's
 * KConfigWatcher picks the change up and calls reloadConfiguration()
 * on the running plugin.
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
    void populateVersions();

    KConfigGroup config;

    KPasswordLineEdit *appKeyEdit = nullptr;
    QComboBox *bibleIdCombo = nullptr;
};
