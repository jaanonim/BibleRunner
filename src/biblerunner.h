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
    static QString getUrl(const QString &book);

    void
    makeBrowserMatch(QList<Plasma::QueryMatch> *matches, const QString &book, const QString &text, const QString &url);

    void makeCopyMatch(QList<Plasma::QueryMatch> *matches, const QString &url);

    static QString getTextOfVerse(const QString &url);
};