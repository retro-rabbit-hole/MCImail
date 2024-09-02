#include <chrono>
#include <ctime>
#include <cwchar>
#include <spanstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "date.hpp"
#include <print>

static const std::unordered_map<std::string_view, std::string> mep2_to_iana = {
    {"AHS", "Etc/GMT+10"}, {"AHD", "Etc/GMT+9"}, {"YST", "Etc/GMT+9"}, {"YDT", "Etc/GMT+8"},
    {"PST", "Etc/GMT+8"},  {"PDT", "Etc/GMT+7"}, {"MST", "Etc/GMT+7"}, {"MDT", "Etc/GMT+6"},
    {"CST", "Etc/GMT+6"},  {"CDT", "Etc/GMT+5"}, {"EST", "Etc/GMT+5"}, {"EDT", "Etc/GMT+4"},
    {"AST", "Etc/GMT+4"},  {"GMT", "Etc/GMT"},   {"BST", "Etc/GMT-1"}, {"WES", "Etc/GMT-1"},
    {"WED", "Etc/GMT-2"},  {"EMT", "Etc/GMT-2"}, {"MTS", "Etc/GMT-3"}, {"MTD", "Etc/GMT-4"},
    {"JST", "Etc/GMT-9"},  {"EAD", "Etc/GMT-10"}};

void Date::parse(std::string_view line) {
    if (line.length() != 29) {
        throw std::invalid_argument("Failed to parse date and time");
    }

    std::ispanstream ss{line};
    std::chrono::local_time<std::chrono::seconds> tp;

    ss >> std::chrono::parse("%a %B %2d, %4Y %2I:%2M %p", tp);

    if (ss.fail()) {
        ss.clear();
        throw std::invalid_argument(
            std::format("Failed to parse date and time at position: {} data: '{}'",
                        std::to_string(ss.tellg()), line));
    }

    std::string_view zone = line.substr(26);
    auto zp = mep2_to_iana.find(zone);
    if (zp == mep2_to_iana.end()) {
        throw std::invalid_argument(std::format("Invalid timezone specifier {}", zone));
    }

    _orig_zone = std::string(zone);
    auto lt = std::chrono::local_time<std::chrono::minutes>{
        std::chrono::duration_cast<std::chrono::minutes>(tp.time_since_epoch())};

    _zone_time = std::chrono::zoned_time{zp->second, lt};
    _utc_time = std::chrono::zoned_time{"Etc/UTC", _zone_time};

#if 0
    auto local_zone = std::chrono::current_zone();
    auto local_time = std::chrono::zoned_time{local_zone, _zt};

    std::cout << std::format("{:%a %b %d, %Y %I:%M %p %Z}", local_time)
              << std::endl;
	for(auto& z: std::chrono::get_tzdb().zones) {
		std::println("Zone: {}", z.name());
	}
#endif
}

const std::string Date::to_utc_string() const {
    return std::format("{:%a %b %d, %Y %I:%M %p %Z}", _utc_time);
}

const std::string Date::to_orig_string() const {
    return std::format("{:%a %b %d, %Y %I:%M %p} {}", _zone_time, _orig_zone);
}

bool Date::operator==(const Date& rhs) const {
    return ((_orig_zone == rhs._orig_zone) && (_utc_time == rhs._utc_time));
}
