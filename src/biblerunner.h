#pragma once

#include <KRunner/AbstractRunner>

class BibleRunner : public Plasma::AbstractRunner {
Q_OBJECT

public:
    BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

    void match(Plasma::RunnerContext &context) override;

    void run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match) override;

protected:
    void init() override;

private:
    QString m_path;
    QString m_triggerWord;
};