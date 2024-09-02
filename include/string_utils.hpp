#ifndef INCLUDE_STRING_UTILS_HPP_
#define INCLUDE_STRING_UTILS_HPP_

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

std::string decode_string(std::string_view sv);

constexpr char lower(const char c) { return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c; }

constexpr unsigned char char_to_hex(const unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    char lc = lower(c);
    if (lc >= 'a' && lc <= 'f') {
        return lc - 'a' + 10;
    }

    throw std::invalid_argument("Input is not a valid hex string");
}

inline bool is_printable(const std::string_view sv) {
    return std::all_of(sv.begin(), sv.end(), [](char c) { return c >= 32 && c <= 126; });
}

inline bool is_printable(const std::string& str) { return is_printable(std::string_view(str)); }

inline bool icompare(const std::string_view haystack, const char* needle) {
    if (haystack.length() < strlen(needle)) {
        return false;
    }

    const char* p = needle;
    for (auto c : haystack) {
        if (!*p)
            return true;

        if (lower(c) != *p++) {
            return false;
        }
    }

    return true;
}

inline void lstrip(std::string_view& sv) {
    while (sv.length() && (sv[0] == ' ' || sv[0] == '\t')) {
        sv.remove_prefix(1);
    }
}

inline void rstrip(std::string_view& sv) {
    while (sv.length() && (sv.back() == ' ' || sv.back() == '\t')) {
        sv.remove_suffix(1);
    }
}

inline void strip(std::string_view& sv) {
    lstrip(sv);
    rstrip(sv);
}

#endif /* INCLUDE_STRING_UTILS_HPP_ */
