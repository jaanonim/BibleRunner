#pragma once

#include <string>

// Book alias lookup lives in a separate translation unit (modules/books.cpp)
// compiled by GCC: its data comes from vendored simdjson's compile-time
// parser using C++26 static reflection (P2996), which clang does not
// implement yet. Nothing else crosses this boundary - just the plain
// libstdc++ std::string below - so the clang-built plugin links the
// GCC-built object without any ABI concerns.
std::string lookupBook(const std::string &raw);
