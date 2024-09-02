#ifndef INCLUDE_DATE_HPP_
#define INCLUDE_DATE_HPP_

#include <chrono>
#include <string_view>

struct Date {
    Date() {}

    void parse(std::string_view line);
    const std::string to_utc_string() const;
    const std::string to_orig_string() const;

    bool operator==(const Date& rhs) const;

    std::string _orig_zone;
    std::chrono::zoned_time<std::chrono::seconds> _zone_time;
    std::chrono::zoned_time<std::chrono::seconds> _utc_time;
};

#endif /* INCLUDE_DATE_HPP_ */
