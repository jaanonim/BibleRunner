module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

export module biblerunner.data.books;

// Book alias tables, loaded at compile time: #embed pulls src/data/books/*.json
// into this translation unit and a constexpr JSON reader folds every file into
// a sorted (alias -> USFM code) table. static_asserts check the result, so
// corrupted JSON or a duplicate alias fails the build instead of shipping.
// Technique: https://lemire.me/blog/2026/06/14/parsing-json-at-compile-time-with-c26-static-reflection/

namespace
{
    // Byte view into compile-time storage. Comparison must match the ordering
    // of the old std::map<std::string, std::string>, i.e. memcmp over
    // unsigned bytes.
    struct View
    {
        const unsigned char *data;
        std::size_t size;
    };

    constexpr bool viewLess(View a, View b)
    {
        return std::lexicographical_compare(a.data,
                                            a.data + a.size,
                                            b.data,
                                            b.data + b.size);
    }

    constexpr bool viewEq(View a, View b)
    {
        return a.size == b.size && std::equal(a.data, a.data + a.size, b.data);
    }

    // Embedded alias tables, one per language file. Order defines conflict
    // handling: when two languages use the same alias for different books,
    // the first file wins (e.g. "jn" stays the Polish JHN, not pt-br's JON).
    constexpr unsigned char TABLE_DE[] = {
#embed "../data/books/de.json"
    };
    constexpr unsigned char TABLE_EN[] = {
#embed "../data/books/en.json"
    };
    constexpr unsigned char TABLE_NOB[] = {
#embed "../data/books/nob.json"
    };
    constexpr unsigned char TABLE_PL[] = {
#embed "../data/books/pl.json"
    };
    constexpr unsigned char TABLE_PT_BR[] = {
#embed "../data/books/pt-br.json"
    };

    constexpr std::array<View, 5> TABLES{
        View{TABLE_DE, sizeof(TABLE_DE)},
        View{TABLE_EN, sizeof(TABLE_EN)},
        View{TABLE_NOB, sizeof(TABLE_NOB)},
        View{TABLE_PL, sizeof(TABLE_PL)},
        View{TABLE_PT_BR, sizeof(TABLE_PT_BR)},
    };

    struct Alias
    {
        View name;         // localized spelling, e.g. "wyjścia"
        View code;         // USFM book code, e.g. "EXO"
        std::size_t order; // file-order index, breaks ties of the sort below
    };

    // JSON reader for the table schema: {"CODE": ["alias", ...], ...}.
    // Strings are plain UTF-8 without escapes; anything else makes the
    // constexpr load fail, which rejects the file at compile time.
    class TableReader
    {
        View src;
        std::size_t pos{0};

        constexpr void skipWhitespace()
        {
            while (pos < src.size && (src.data[pos] == ' ' || src.data[pos] == '\n' || src.data[pos] == '\r' || src.data[pos] == '\t'))
            {
                ++pos;
            }
        }

        constexpr bool at(unsigned char c)
        {
            skipWhitespace();
            return pos < src.size && src.data[pos] == c;
        }

        constexpr bool expect(unsigned char c)
        {
            if (!at(c))
            {
                return false;
            }
            ++pos;
            return true;
        }

        constexpr View parseString(bool &ok)
        {
            if (!expect('"'))
            {
                ok = false;
                return {};
            }
            const std::size_t begin = pos;
            while (pos < src.size && src.data[pos] != '"')
            {
                if (src.data[pos] == '\\')
                {
                    // escape sequences are outside the schema; the caller
                    // must keep aliases plain UTF-8
                    ok = false;
                    return {};
                }
                ++pos;
            }
            if (!expect('"'))
            {
                ok = false;
                return {};
            }
            return View{src.data + begin, pos - 1 - begin};
        }

    public:
        constexpr explicit TableReader(View table) : src{table} {}

        // Calls add(alias, usfmCode) for every string of every array;
        // returns false when the file does not fit the schema.
        template <typename Add>
        constexpr bool forEachAlias(Add &&add)
        {
            if (!expect('{'))
            {
                return false;
            }
            if (at('}'))
            {
                ++pos;
            }
            else
            {
                while (true)
                {
                    bool ok = true;
                    View code = parseString(ok); // object key = USFM code
                    if (!ok || !expect(':') || !expect('['))
                    {
                        return false;
                    }
                    while (true)
                    {
                        if (at(']'))
                        {
                            ++pos;
                            break;
                        }
                        ok = true;
                        View name = parseString(ok);
                        if (!ok)
                        {
                            return false;
                        }
                        add(name, code);
                        if (at(','))
                        {
                            ++pos;
                            continue;
                        }
                        if (!expect(']'))
                        {
                            return false;
                        }
                        break;
                    }
                    if (at(','))
                    {
                        ++pos;
                        continue;
                    }
                    if (!expect('}'))
                    {
                        return false;
                    }
                    break;
                }
            }
            skipWhitespace();
            return pos == src.size;
        }
    };

    // Tables are parsed twice: once to size the buffer, once to fill it.
    struct AliasCount
    {
        std::size_t value{0};
        bool ok{false};
    };

    constexpr AliasCount countAliases()
    {
        std::size_t count = 0;
        for (const View &table : TABLES)
        {
            if (!TableReader{table}.forEachAlias([&count](View, View)
                                                 { ++count; }))
            {
                return {};
            }
        }
        return AliasCount{count, true};
    }

    constexpr AliasCount COUNTED = countAliases();

    // Corrupted alias JSON fails the build here.
    static_assert(COUNTED.ok, "src/data/books/*.json does not parse");

    constexpr std::size_t ALIAS_COUNT = COUNTED.value;

    struct AliasTable
    {
        std::array<Alias, ALIAS_COUNT> aliases{};
        bool ok{false};
    };

    constexpr AliasTable collectAliases()
    {
        AliasTable parsed{};
        bool ok = true;
        std::size_t order = 0;
        for (const View &table : TABLES)
        {
            ok = ok && TableReader{table}.forEachAlias(
                           [&parsed, &order](View name, View code)
                           {
                               parsed.aliases[order] = Alias{name, code, order};
                               ++order;
                           });
        }
        parsed.ok = ok && order == parsed.aliases.size();
        return parsed;
    }

    constexpr AliasTable COLLECTED = collectAliases();

    static_assert(COLLECTED.ok, "src/data/books/*.json does not parse");
    constexpr std::array<Alias, ALIAS_COUNT> ALIASES = COLLECTED.aliases;

    // Sort by name; equal names keep their file order, so when two languages
    // map the same alias to different codes the first file wins.
    constexpr bool aliasLess(const Alias &a, const Alias &b)
    {
        if (!viewEq(a.name, b.name))
        {
            return viewLess(a.name, b.name);
        }
        return a.order < b.order;
    }

    template <std::size_t N>
    constexpr std::array<Alias, N> sortedAliases(std::array<Alias, N> aliases)
    {
        std::sort(aliases.begin(), aliases.end(), aliasLess);
        return aliases;
    }

    constexpr std::array<Alias, ALIAS_COUNT> SORTED = sortedAliases(ALIASES);

    // The table keeps only the first occurrence of each alias.
    template <std::size_t N>
    constexpr std::size_t countBooks(const std::array<Alias, N> &sorted)
    {
        std::size_t unique = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (i == 0 || !viewEq(sorted[i].name, sorted[i - 1].name))
            {
                ++unique;
            }
        }
        return unique;
    }

    constexpr std::size_t BOOK_COUNT = countBooks(SORTED);

    template <std::size_t N, std::size_t M>
    constexpr std::array<Alias, M> buildBooks(const std::array<Alias, N> &sorted)
    {
        std::array<Alias, M> books{};
        std::size_t next = 0;
        for (std::size_t i = 0; i < N; ++i)
        {
            if (i == 0 || !viewEq(sorted[i].name, sorted[i - 1].name))
            {
                books[next++] = sorted[i];
            }
        }
        return books;
    }

    // Sorted (alias -> USFM code) lookup table.
    constexpr std::array<Alias, BOOK_COUNT> BOOKS = buildBooks<ALIAS_COUNT, BOOK_COUNT>(SORTED);

    // Compile-time validation of the generated table.
    static_assert(std::is_sorted(BOOKS.begin(),
                                 BOOKS.end(),
                                 [](const Alias &a, const Alias &b)
                                 { return viewLess(a.name, b.name); }),
                  "book alias table is not sorted");
    static_assert(std::adjacent_find(BOOKS.begin(),
                                     BOOKS.end(),
                                     [](const Alias &a, const Alias &b)
                                     { return viewEq(a.name, b.name); }) == BOOKS.end(),
                  "book alias table contains a duplicate alias");
    static_assert(BOOKS.size() != 0, "book alias table is empty");
} // namespace

export std::string lookupBook(const std::string &raw)
{
    const View name{reinterpret_cast<const unsigned char *>(raw.data()), raw.size()};
    const auto it = std::lower_bound(BOOKS.begin(),
                                     BOOKS.end(),
                                     name,
                                     [](const Alias &alias, const View &key)
                                     { return viewLess(alias.name, key); });
    if (it != BOOKS.end() && viewEq(it->name, name))
    {
        std::string code;
        for (std::size_t i = 0; i < it->code.size; ++i)
        {
            code.push_back(static_cast<char>(it->code.data[i]));
        }
        return code;
    }
    return {};
}
