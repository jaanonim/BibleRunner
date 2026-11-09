#pragma once

#include <string>

// compiled by g++, not clang: needs C++26 reflection (simdjson P2996).
// only this plain libstdc++ std::string crosses the compiler boundary.
std::string lookupBook(const std::string &raw);
