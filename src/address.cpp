#include <format>
#include <optional>
#include <print>
#include <regex>
#include <string_view>

#include "address.hpp"
#include "mep2_errors.hpp"
#include "string_utils.hpp"

bool is_mciid(std::string_view line) {
    // MCI IDs are in the form of:
    // 123-4567, 123-456-7890, 1234567, 1234567890

    static const std::regex mciid_regex(R"(^(\d{3}-\d{4}|\d{3}-\d{3}-\d{4}|\d{7}|\d{10})$)");
    return std::regex_match(line.begin(), line.end(), mciid_regex);
}

std::optional<std::string_view> parse_mciid(std::string_view line) {
    bool explicit_mciid = false;
    if (line.starts_with("MCI ID:")) {
        line.remove_prefix(7);
        lstrip(line);

        explicit_mciid = true;
    }

    if (is_mciid(line)) {
        return line;
    }

    if (explicit_mciid) {
        throw PduMalformedDataError("Invalid MCI ID after MCI ID:");
    }

    return std::nullopt;
}

std::string canonicalize_mciid(std::string_view line) {
    if (!is_mciid(line)) {
        throw std::invalid_argument("Invalid MCI ID format");
    }

    // In the form of 123-4567
    // Note that we can't exit here if size is 12 as an ID in the form of
    // 000-123-4567 is not in canonical form
    if (line.length() == 8) {
        return std::string(line);
    }

    // In the form of 123-456-7890 or 1234567890
    // Strip off any leading 000 or 0000-
    if (line.length() >= 10 && line.starts_with("000")) {
        line.remove_prefix(line[3] == '-' ? 4 : 3);
    }

    // All done, if we are size 8 or 12 at this point we have
    // either 123-4567 or 123-456-7890
    if (line.length() == 8 || line.length() == 12) {
        return std::string(line);
    }

    // We have numbers only, add the dashes
    if (line.length() == 7) {
        return std::format("{}-{}", line.substr(0, 3), line.substr(3));
    } else {
        return std::format("{}-{}-{}", line.substr(0, 3), line.substr(3, 3), line.substr(6));
    }
}

bool RawAddress::operator==(const RawAddress& rhs) const {
    if (_name != rhs._name)
        return false;
    if (_id != rhs._id)
        return false;
    if (_organization != rhs._organization)
        return false;
    if (_location != rhs._location)
        return false;
    if (_unresolved_org_loc_1 != rhs._unresolved_org_loc_1)
        return false;
    if (_unresolved_org_loc_2 != rhs._unresolved_org_loc_2)
        return false;
    if (_ems != rhs._ems)
        return false;

    if (_mbx.size() != rhs._mbx.size())
        return false;

    for (size_t i = 0; i < _mbx.size(); ++i) {
        if (_mbx[i] != rhs._mbx[i])
            return false;
    }

    if (_board != rhs._board)
        return false;
    if (_instant != rhs._instant)
        return false;
    if (_list != rhs._list)
        return false;
    if (_owner != rhs._owner)
        return false;

    return true;
}

void RawAddress::parse_org_or_loc(std::string_view line) {
    if (is_mciid(line)) {
        throw PduMalformedDataError("Location/Organization cannot be an MCI ID");
    }

    if (line.starts_with("Loc:")) {
        line.remove_prefix(4);
        strip(line);

        if (!line.length()) {
            throw PduMalformedDataError("Location cannot be empty");
        }
        _location = line;
    } else if (line.starts_with("Org:")) {
        line.remove_prefix(4);
        strip(line);

        if (!line.length()) {
            throw PduMalformedDataError("Organization cannot be empty");
        }
        _organization = line;
    } else {
        if (!line.length()) {
            throw PduMalformedDataError("Organization/Location cannot be empty");
        }

        if (_unresolved_org_loc_1.empty()) {
            _unresolved_org_loc_1 = line;
        } else {
            _unresolved_org_loc_2 = line;
        }
    }
}

void RawAddress::parse_options(std::string_view& line) {
    // Line is rstipped so the last character should be a ')' if this has
    // options
    if (!line.ends_with(')'))
        return;

    if (std::count(line.cbegin(), line.cend(), '(') != 1) {
        throw PduMalformedDataError("Malformed options, too many parenthesis");
    }

    if (std::count(line.cbegin(), line.cend(), ')') != 1) {
        throw PduMalformedDataError("Malformed options, too many parenthesis");
    }

    size_t options_start = line.find_first_of('(');
    size_t options_len = line.length() - options_start;

    // options minus enclosing parenthesis
    std::string_view options = line.substr(options_start + 1, options_len - 2);
    // whitespace is allowed around options
    strip(options);

    // further parsing steps shouldn't see options
    line.remove_suffix(options_len);
    // and remove any potential whitespace between address and options
    rstrip(line);

    while (options.length()) {
        // std::println("Parsing options {}", options);
        size_t delim = options.find_first_of(',');

        if (delim == options.length() - 1) {
            throw PduMalformedDataError("Malformed options, trailing comma");
        }

        std::string_view option = options;
        if (delim != std::string_view::npos) {
            option.remove_suffix(options.length() - delim);
            options.remove_prefix(delim + 1);
        } else {
            options.remove_prefix(option.length());
        }

        if (!option.length()) {
            throw PduMalformedDataError("Malformed options, empty option");
        }

        // Whitespace should be ignored
        strip(option);

        // std::println("consider option {}", option);

        if (option == "BOARD") {
            _board = true;
        } else if (option == "INSTANT") {
            _instant = true;
        } else if (option == "LIST") {
            _list = true;
        } else if (option == "OWNER") {
            _owner = true;
        } else if (option == "ONITE") {
            _onite = true;
        } else if (option == "PRINT") {
            _print = true;
        } else if (option == "RECEIPT") {
            _receipt = true;
        } else if (option == "NO RECEIPT") {
            _no_receipt = true;
        } else {
            throw PduMalformedDataError(
                std::format("Malformed options, unknown option '{}'", option));
        }
        _has_options = true;
    }
}

void RawAddress::parse_first_line(std::string_view line) {
    size_t num_slashes = std::count(line.cbegin(), line.cend(), '/');
    if (num_slashes > 2) {
        throw PduMalformedDataError("Too many fields");
    }

    rstrip(line);

    if (!line.length()) {
        throw PduMalformedDataError("Empty address");
    }

    // Check to see if we have recipient options
    parse_options(line);

    // No slashes, must just be a name or id.
    if (num_slashes == 0) {
        auto mciid = parse_mciid(line);
        if (mciid.has_value()) {
            _id = canonicalize_mciid(*mciid);
        } else {
            if (!line.length()) {
                throw PduMalformedDataError("Name cannot be empty");
            }
            _name = line;
        }

        return;
    }

    size_t first_slash = line.find('/');
    std::string_view first_part = line.substr(0, first_slash);
    if (!first_part.length()) {
        throw PduMalformedDataError("Name/ID field invalid");
    }
    rstrip(first_part);
    auto mciid = parse_mciid(first_part);
    if (mciid.has_value()) {
        // Handle "MCIID / Org or Loc"
        _id = canonicalize_mciid(*mciid);
    } else {
        // Handle "Name / MCIid" or "Name / Org or Loc"
        if (!line.length()) {
            throw PduMalformedDataError("Name cannot be empty");
        }
        _name = first_part;
    }

    line.remove_prefix(first_slash + 1);
    if (!line.length()) {
        throw PduMalformedDataError("First Organization/Location field invalid");
    }

    strip(line);

    if (num_slashes == 1) {
        if (_id.empty()) {
            auto mciid = parse_mciid(line);
            // Deal with "User name / MCIID"
            if (mciid.has_value()) {
                _id = canonicalize_mciid(*mciid);
                return;
            }
        }

        // Deal with "MCIID / Org or Loc"
        parse_org_or_loc(line);
        return;
    }

    // Deal with Username, id / Org or Loc / Org or Loc
    size_t second_slash = line.find('/');
    std::string_view second_part = line.substr(0, second_slash);
    std::string_view third_part = line.substr(second_slash + 1);
    strip(second_part);
    strip(third_part);

    if (is_mciid(second_part) || is_mciid(third_part)) {
        throw PduMalformedDataError("Organization/Location cannot be an MCI ID");
    }

    parse_org_or_loc(second_part);
    parse_org_or_loc(third_part);
}

void RawAddress::parse_field(std::string_view field, std::string_view information) {
    // Shortest theoretical field is MBX:

    if (field.length() < 4) {
        throw PduMalformedDataError("Unknown field type");
    }

    if (icompare(field, "ems:")) {
        if (!_ems.empty()) {
            throw PduMalformedDataError("Multiple EMS directive in address");
        }

        if (!information.length()) {
            throw PduMalformedDataError("EMS cannot be empty");
        }

        _ems = information;
    } else if (icompare(field, "mbx:")) {
        if (_ems.empty()) {
            throw PduMalformedDataError("MBX without EMS");
        }

        if (!information.length()) {
            throw PduMalformedDataError("MBX cannot be empty");
        }

        _mbx.push_back(std::string(information));

        size_t mbx_len = 0;
        for (auto& m : _mbx) {
            mbx_len += m.length();
        }

        if (mbx_len > 305) {
            throw PduMalformedDataError("MBX routing info larger than 305 characters");
        }
    } else {
        throw PduMalformedDataError(std::format("Unknown address field {}", field));
    }
}

const std::string RawAddress::str() const {
    std::stringstream ss;
    if (_name.empty()) {
        ss << _id;
    } else {
        ss << _name;

        if (!_id.empty()) {
            ss << " / " << _id;
        } else {
            if (!_location.empty())
                ss << " / Loc: " << _location;
            if (!_organization.empty())
                ss << " / Org: " << _organization;

            if (!_unresolved_org_loc_1.empty())
                ss << " / " << _unresolved_org_loc_1;
            if (!_unresolved_org_loc_2.empty())
                ss << " / " << _unresolved_org_loc_2;
        }
    }

    static const std::array<std::pair<bool RawAddress::*, const char*>, 8> options = {
        {{&RawAddress::_board, "BOARD"},
         {&RawAddress::_instant, "INSTANT"},
         {&RawAddress::_list, "LIST"},
         {&RawAddress::_owner, "OWNER"},
         {&RawAddress::_onite, "ONITE"},
         {&RawAddress::_print, "PRINT"},
         {&RawAddress::_receipt, "RECEIPT"},
         {&RawAddress::_no_receipt, "NO RECEIPT"}}};

    if (_has_options) {
        bool first = true;
        ss << " (";
        for (auto v : options) {
            if (this->*v.first) {
                if (!first)
                    ss << ", ";
                else
                    first = false;
                ss << v.second;
            }
        }
        ss << ")";
    }

    return ss.str();
}
