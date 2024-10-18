#ifndef INCLUDE_ADDRESS_HPP_
#define INCLUDE_ADDRESS_HPP_

#include <string>
#include <string_view>
#include <vector>

bool is_mciid(std::string_view line);
std::string canonicalize_mciid(std::string_view line);

struct RawAddress {
    void parse_org_or_loc(std::string_view line);
    void parse_options(std::string_view& line);
    void parse_first_line(std::string_view line);
    void parse_field(std::string_view field, std::string_view information);

    const std::string str() const;
    bool operator==(const RawAddress& rhs) const;

    std::string _name{};
    std::string _id{};
    std::string _organization{};
    std::string _location{};
    std::string _unresolved_org_loc_1{};
    std::string _unresolved_org_loc_2{};

    std::string _alert{};

    std::string _ems{};
    std::vector<std::string> _mbx{};

	bool _has_options{false};
    bool _board{false};
    bool _instant{false};
    bool _list{false};
    bool _owner{false};
    bool _onite{false};
    bool _print{false};
    bool _receipt{false};
    bool _no_receipt{false};
};

#endif /* INCLUDE_ADDRESS_HPP_ */
