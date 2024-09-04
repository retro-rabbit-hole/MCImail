#pragma once

#include <chrono>
#include <string_view>

struct Date {
    Date() {}

    void parse(std::string_view line);
    const std::string to_gmt_string() const;
    const std::string to_orig_string() const;

    bool operator==(const Date& rhs) const;

    std::string _orig_zone;
    std::chrono::zoned_time<std::chrono::seconds> _zone_time;
    // Mep2 clients don't know about UTC, only GMT.
    std::chrono::zoned_time<std::chrono::seconds> _gmt_time;
};
