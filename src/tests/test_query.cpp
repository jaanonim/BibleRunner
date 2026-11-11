#include <boost/test/unit_test.hpp>

#include "boost_printers.hpp"

#include <QString>
#include <QUrl>

#include <optional>
#include <string>
#include <vector>

#include "modules/books.h"

import biblerunner.query;

namespace
{
    // as passed by the runner via DEFAULT_BIBLE_ID in config.cppm
    const QString bibleId = QStringLiteral("2095");

    std::optional<QueryMatchInfo> parse(const QString &raw)
    {
        return parseQuery(raw, bibleId);
    }
} // namespace

BOOST_AUTO_TEST_SUITE(query)

BOOST_AUTO_TEST_CASE(accepted_references)
{
    struct Expectation
    {
        QString raw;
        QString book;    // USFM code
        QString text;    // human text
        QString passage; // USFM passage for the API
        QString url;
    };

    const std::vector<Expectation> accepted = {
        // typical mixed-case English input
        {QStringLiteral("John 3:16"), QStringLiteral("JHN"), QStringLiteral("3:16"), QStringLiteral("JHN.3.16"),
         QStringLiteral("https://www.bible.com/bible/2095/JHN.3.16")},
        // lowercase book name, digits-only chapter verse
        {QStringLiteral("genesis 1:1"), QStringLiteral("GEN"), QStringLiteral("1:1"), QStringLiteral("GEN.1.1"),
         QStringLiteral("https://www.bible.com/bible/2095/GEN.1.1")},
        // chapter without verse
        {QStringLiteral("Psalm 119"), QStringLiteral("PSA"), QStringLiteral("119"), QStringLiteral("PSA.119"),
         QStringLiteral("https://www.bible.com/bible/2095/PSA.119")},
        // digit-led alias: spaces are stripped before the lookup, so only
        // the Polish no-space "1kor" resolves
        {QStringLiteral("1kor 13:4-7"), QStringLiteral("1CO"), QStringLiteral("13:4-7"), QStringLiteral("1CO.13.4-7"),
         QStringLiteral("https://www.bible.com/bible/2095/1CO.13.4-7")},
        // digit-led letter alias
        {QStringLiteral("3 john 1:14"), QStringLiteral("3JN"), QStringLiteral("1:14"), QStringLiteral("3JN.1.14"),
         QStringLiteral("https://www.bible.com/bible/2095/3JN.1.14")},
        // no space between book and verse
        {QStringLiteral("John3:16"), QStringLiteral("JHN"), QStringLiteral("3:16"), QStringLiteral("JHN.3.16"),
         QStringLiteral("https://www.bible.com/bible/2095/JHN.3.16")},
        // verse range after the chapter-verse separator
        {QStringLiteral("John 3:16-18"), QStringLiteral("JHN"), QStringLiteral("3:16-18"), QStringLiteral("JHN.3.16-18"),
         QStringLiteral("https://www.bible.com/bible/2095/JHN.3.16-18")},
        // non-ASCII alias of the Polish book table
        {QStringLiteral("wyjścia 3:14"), QStringLiteral("EXO"), QStringLiteral("3:14"), QStringLiteral("EXO.3.14"),
         QStringLiteral("https://www.bible.com/bible/2095/EXO.3.14")},
    };

    for (const Expectation &want : accepted)
    {
        BOOST_TEST_CONTEXT("query: " << want.raw.toStdString())
        {
            const std::optional<QueryMatchInfo> got = parse(want.raw);
            BOOST_REQUIRE(got.has_value());
            BOOST_TEST(got->book == want.book);
            BOOST_TEST(got->text == want.text);
            BOOST_TEST(got->passage == want.passage);
            BOOST_TEST(got->url.toString() == want.url);
        }
    }
}

BOOST_AUTO_TEST_CASE(rejected_fragments)
{
    const std::vector<QString> rejected = {
        QString(),                    // nothing typed
        QStringLiteral("?"),          // KRunner command prefix
        QStringLiteral("?John 3:16"), // KRunner command prefix
        QStringLiteral("John"),       // book name without chapter/verse
        QStringLiteral("3:16"),       // numbers without a book name
        QStringLiteral("xyzzy 3:16"), // book name not in the alias tables
        QStringLiteral("John 1234:16"),    // chapter too long
        QStringLiteral("words John 3:16"), // match would not cover the query
        QStringLiteral("John 3:16 words"),
        QStringLiteral("John 3-16"),    // a dash is only accepted as a range
                                        // separator after : , or .
        QStringLiteral("John 3:16,18"), // second separator not consumed
    };

    for (const QString &raw : rejected)
    {
        BOOST_TEST_CONTEXT("query: " << raw.toStdString())
        {
            BOOST_TEST(!parse(raw).has_value());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(book_aliases)

BOOST_AUTO_TEST_CASE(lookup_returns_usfm_code)
{
    BOOST_TEST(lookupBook("john") == "JHN");
    BOOST_TEST(lookupBook("genesis") == "GEN");
    BOOST_TEST(lookupBook("psalm") == "PSA");
    BOOST_TEST(lookupBook("1co") == "1CO"); // demangled digit-led key
    BOOST_TEST(lookupBook("3jn") == "3JN"); // demangled digit-led key
    // UTF-8 alias of the Polish book table
    BOOST_TEST(lookupBook("wyjścia") == "EXO");
}

BOOST_AUTO_TEST_CASE(lookup_unknown_or_empty)
{
    BOOST_TEST(lookupBook("xyzzy").empty());
    BOOST_TEST(lookupBook("").empty());
}

BOOST_AUTO_TEST_SUITE_END()
