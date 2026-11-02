#define SIMDJSON_STATIC_REFLECTION 1
#include "simdjson.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>

// CMake-generated: BOOK_TABLE_COUNT, the per-file BOOK_TABLE_<i> embedded C
// strings with their BOOK_DOC_<i> documents, and the BOOK_DOCS pack macro
// (one book alias table per src/data/books/*.json, in file-name order).
#include "books_tables.h"

#include "books.h"

namespace
{
    // One lookup row: alias spelling -> 3-letter USFM book code.
    struct Entry
    {
        std::string_view name; // localized spelling, e.g. "wyjścia"
        std::string_view code; // USFM book code, e.g. "EXO"
        std::size_t order;     // table-order index, breaks equal-name ties
    };

    // parse_json generates a structure where every JSON key becomes a data
    // member and every string of its array one alias. The reflection helpers
    // below see the document through a template reference parameter: an
    // expansion statement cannot read an ordinary function parameter as a
    // constant expression.
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

    // Sums the alias counts of the BOOK_DOCS tables. A unary fold needs no
    // recursion helper: it also folds an empty pack to 0, which yields an
    // empty table when the src/data/books/*.json glob matched nothing.
    template <auto &...DOCS>
    consteval std::size_t countAll() { return (countDoc<DOCS>() + ...); }

    constexpr std::size_t ALIAS_COUNT = countAll<BOOK_DOCS>();

    // Extracts the USFM code from a data-member identifier. Digit-led book
    // codes ("1SA") were embedded mangled to "_1SA", since identifiers cannot
    // start with a digit. The demangled text is promoted to static storage so
    // the resulting string_view stays valid at runtime.
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

    // Appends a per-file chunk to the master table. The master index replaces
    // the per-file chunk index, so equal-name ties are broken in file order,
    // i.e. the first file wins.
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

    // Chains the BOOK_DOCS tables into one master table in file order. The
    // comma fold appends one table per document sequentially and also works
    // for an empty pack (folding to void).
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
        {
            throw "alias table book count mismatch";
        }
        return out;
    }

    // Ordering must match the old std::map<std::string, std::string> lookup
    // table, i.e. lexicographical over unsigned bytes; std::string_view and
    // std::char_traits<char> already compare that way.
    constexpr bool entryLess(const Entry &a, const Entry &b)
    {
        if (a.name != b.name)
            return a.name < b.name;
        return a.order < b.order;
    }

    constexpr auto sortedEntries(std::array<Entry, ALIAS_COUNT> entries)
    {
        std::sort(entries.begin(), entries.end(), entryLess);
        return entries;
    }

    constexpr std::size_t countBooks(const std::array<Entry, ALIAS_COUNT> &sorted)
    {
        std::size_t unique = 0;
        for (std::size_t i = 0; i < sorted.size(); ++i)
        {
            if (i == 0 || sorted[i].name != sorted[i - 1].name)
                ++unique;
        }
        return unique;
    }

    template <std::size_t M>
    consteval std::array<Entry, M> dedupEntries(const std::array<Entry, ALIAS_COUNT> &sorted)
    {
        std::array<Entry, M> kept{};
        std::size_t next = 0;
        for (std::size_t i = 0; i < sorted.size(); ++i)
        {
            if (i == 0 || sorted[i].name != sorted[i - 1].name)
                kept[next++] = sorted[i];
        }
        if (next != M)
            throw "book alias table book count mismatch";

        return kept;
    }

    constexpr std::array<Entry, ALIAS_COUNT> SORTED = sortedEntries(collectAll());
    static_assert(ALIAS_COUNT != 0, "book alias table is empty");
    constexpr std::size_t BOOK_COUNT = countBooks(SORTED);
    constexpr std::array<Entry, BOOK_COUNT> BOOKS = dedupEntries<BOOK_COUNT>(SORTED);

    static_assert(std::adjacent_find(BOOKS.begin(),
                                     BOOKS.end(),
                                     [](const Entry &a, const Entry &b)
                                     { return a.name == b.name; }) == BOOKS.end(),
                  "book alias table contains a duplicate alias");
} // namespace

std::string lookupBook(const std::string &raw)
{
    const std::string_view name{raw.data(), raw.size()};
    const auto it = std::lower_bound(BOOKS.begin(),
                                     BOOKS.end(),
                                     name,
                                     [](const Entry &entry, const std::string_view &key)
                                     { return entry.name < key; });
    if (it != BOOKS.end() && it->name == name)
    {
        return std::string(it->code);
    }
    return {};
}
