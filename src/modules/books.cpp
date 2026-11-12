#define SIMDJSON_STATIC_REFLECTION 1
#include "simdjson.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>

// CMake-generated alias tables, see src/CMakeLists.txt
#include "books_tables.h"

#include "books.h"

namespace
{
    struct Entry
    {
        std::string_view name; // alias, e.g. "wyjścia"
        std::string_view code; // USFM code, e.g. "EXO"
        std::size_t order;     // file order, breaks name ties
    };

    // every JSON key becomes a member, its array strings the aliases;
    // the doc must be a template parameter (expansion statements can't
    // read ordinary function parameters)
    template <auto &DOC>
    consteval std::size_t countDoc()
    {
        std::size_t count = 0;
        template for (constexpr auto md : std::define_static_array(
                          std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<decltype(DOC)>,
                                                               std::meta::access_context::current())))
        {
            template for (constexpr auto alias : DOC.[:md:])
            {
                ++count;
            }
        }
        return count;
    }

    // empty pack folds to 0
    template <auto &...DOCS>
    consteval std::size_t countAll() { return (countDoc<DOCS>() + ...); }

    constexpr std::size_t ALIAS_COUNT = countAll<BOOK_DOCS>();

    // digit-led keys were mangled ("_1SA"); static storage keeps the view valid
    consteval std::string_view demangleCode(std::string_view key)
    {
        std::size_t skip = 0;
        if (key.size() > 1 && key[0] == '_' && key[1] >= '0' && key[1] <= '9')
            skip = 1;
        return std::string_view(std::define_static_string(key.substr(skip)), key.size() - skip);
    }

    template <auto &DOC>
    consteval auto collectDoc()
    {
        std::array<Entry, countDoc<DOC>()> out{};
        std::size_t i = 0;
        template for (constexpr auto md : std::define_static_array(
                          std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<decltype(DOC)>,
                                                               std::meta::access_context::current())))
        {
            const std::string_view code = demangleCode(std::meta::identifier_of(md));
            template for (constexpr auto alias : DOC.[:md:])
            {
                out[i].name = std::string_view(alias);
                out[i].code = code;
                out[i].order = i;
                ++i;
            }
        }
        return out;
    }

    template <std::size_t N>
    consteval void appendChunk(std::array<Entry, ALIAS_COUNT> &out, std::size_t &pos,
                               const std::array<Entry, N> &chunk)
    {
        for (std::size_t i = 0; i < chunk.size(); ++i)
        {
            out[pos] = chunk[i];
            out[pos].order = pos;
            ++pos;
        }
    }

    template <auto &...DOCS>
    consteval void chainTables(std::array<Entry, ALIAS_COUNT> &out, std::size_t &pos)
    {
        (appendChunk(out, pos, collectDoc<DOCS>()), ...);
    }

    consteval std::array<Entry, ALIAS_COUNT> collectAll()
    {
        std::array<Entry, ALIAS_COUNT> out{};
        std::size_t pos = 0;
        chainTables<BOOK_DOCS>(out, pos);
        if (pos != out.size())
            throw "alias table book count mismatch";

        return out;
    }

    // byte-wise, matching the old std::map lookup
    constexpr bool entryLess(const Entry &a, const Entry &b)
    {
        if (a.name != b.name)
            return a.name < b.name;
        return a.order < b.order;
    }

    constexpr auto sortedEntries(std::array<Entry, ALIAS_COUNT> entries)
    {
        std::ranges::sort(entries, entryLess);
        return entries;
    }

    // takes a mutable copy: ranges::unique must permute the entries
    constexpr std::size_t countBooks(std::array<Entry, ALIAS_COUNT> sorted)
    {
        auto end = std::ranges::unique(sorted,
                                       [](const Entry &a, const Entry &b)
                                       { return a.name == b.name; })
                       .begin();

        return std::distance(sorted.begin(), end);
    }

    template <std::size_t M>
    consteval std::array<Entry, M> dedupEntries(std::array<Entry, ALIAS_COUNT> sorted)
    {
        auto end = std::ranges::unique(sorted, {}, &Entry::name).begin();

        if (std::ranges::distance(sorted.begin(), end) != M)
            throw "book alias table book count mismatch";

        std::array<Entry, M> kept{};
        std::ranges::copy(sorted.begin(), end, kept.begin());

        return kept;
    }

    constexpr std::array<Entry, ALIAS_COUNT> SORTED = sortedEntries(collectAll());
    static_assert(ALIAS_COUNT != 0, "book alias table is empty");
    constexpr std::size_t BOOK_COUNT = countBooks(SORTED);
    constexpr std::array<Entry, BOOK_COUNT> BOOKS = dedupEntries<BOOK_COUNT>(SORTED);

    static_assert(std::ranges::adjacent_find(BOOKS,
                                             [](const Entry &a, const Entry &b)
                                             { return a.name == b.name; }) == BOOKS.end(),
                  "book alias table contains a duplicate alias");
} // namespace

std::string lookupBook(const std::string &raw)
{
    const std::string_view name{raw.data(), raw.size()};
    const auto it = std::ranges::lower_bound(BOOKS, name, {}, &Entry::name);
    if (it != BOOKS.end() && it->name == name)
        return std::string(it->code);
    return {};
}
