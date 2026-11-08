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
    // tags stored in the match data and interpreted by run()
    enum MatchAction
    {
        Copy = 0,
        Browser = 1,
        SetKey = 2,
    };

    void
    makeBrowserMatch(QList<KRunner::QueryMatch> *matches, const QString &book, const QString &text, const QString &url);

    void makeCopyMatch(QList<KRunner::QueryMatch> *matches, const QString &passage, const QString &url);

    // hint match shown instead of the copy match while no API key is set
    void makeSetKeyMatch(QList<KRunner::QueryMatch> *matches);
};
