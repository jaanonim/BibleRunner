#pragma once

#include <KRunner/AbstractRunner>

class BibleRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    BibleRunner(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;
    void reloadConfiguration() override;

protected:
    void init() override;

private:
    void
    makeBrowserMatch(QList<KRunner::QueryMatch> *matches, const QString &book, const QString &text, const QString &url);

    void makeCopyMatch(QList<KRunner::QueryMatch> *matches, const QString &passage, const QString &url);
};
