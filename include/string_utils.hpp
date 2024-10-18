#pragma once

#include <algorithm>
#include <cstring>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>

std::string decode_string(std::string_view sv);
std::string encode_string(std::string_view sv);

// Our own because we don't want any locale interpretations
constexpr char lower(const char c) { return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c; }

constexpr unsigned char hex_to_char(const unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    char lc = lower(c);
    if (lc >= 'a' && lc <= 'f') {
        return lc - 'a' + 10;
    }

    throw std::invalid_argument("Input is not a valid hex character");
}

constexpr unsigned char char_to_hex(const unsigned char c) {
    return c < 10 ? '0' + c : 'A' + (c - 10);
};

inline bool is_printable(const std::string_view sv) {
    return std::all_of(sv.begin(), sv.end(), [](char c) { return c >= 32 && c <= 126; });
}

inline bool is_numeric(const std::string_view sv) {
    return std::all_of(sv.begin(), sv.end(), [](char c) { return c >= '0' && c <= '9'; });
}

inline bool is_printable(const std::string& str) { return is_printable(std::string_view(str)); }

inline bool icompare(std::string_view haystack, std::string_view needle) {
    if (haystack.length() < needle.length()) {
        return false;
    }

    haystack = haystack.substr(0, needle.length());
    return std::ranges::equal(haystack | std::views::transform(lower), needle);
}

static constexpr std::string_view whitespace = " \t";

inline void lstrip(std::string_view& sv) {
    size_t start = sv.find_first_not_of(whitespace);
    if (start == std::string_view::npos) {
        // Oh no! All whitespace
        sv = std::string_view();
    } else {
        sv.remove_prefix(start);
    }
}

inline void rstrip(std::string_view& sv) {
    size_t end = sv.find_last_not_of(whitespace);
    if (end == std::string_view::npos) {
        // Oh no! All whitespace
        sv = std::string_view();
    } else {
        // end is the index, so +1 to make it a length
        sv = sv.substr(0, end + 1);
    }
}

inline void strip(std::string_view& sv) {
    size_t start = sv.find_first_not_of(whitespace);

    if (start == std::string_view::npos) {
        // Oh no! All whitespace
        sv = std::string_view();
    } else {
        size_t end = sv.find_last_not_of(whitespace);
        // end is the index, so +1 to make it a length
        sv = sv.substr(start, (end - start) + 1);
    }
}
