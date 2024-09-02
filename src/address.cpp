#include <format>
#include <print>
#include <string_view>

#include "address.hpp"
#include "mep2_errors.hpp"
#include "string_utils.hpp"

bool is_mciid(std::string_view line) {
    // MCI IDs are in the form of:
    // 123-4567, 123-456-7890, 1234567, 1234567890

    switch (line.length()) {
    case 7:
    case 8:
    case 10:
    case 12: {
        for (size_t i = 0; i < line.length(); ++i) {
            if (line[i] < '0' || line[i] > '9') {
                if (line[i] == '-' && (i == 3 || i == 7)) {
                    continue;
                }
                return false;
            }
        }
        break;
    }

    default:
        return false;
    }

    return true;
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

    // We have a string of numbers only
    std::string retval;
    retval.reserve(12);
    // Add first 3 digits
    retval += line.substr(0, 3);
    retval += '-';

    if (line.length() == 7) {
        // Add last 4 digits
        retval += line.substr(3);
    } else {
        // Add second group of 3 digits
        retval += line.substr(3, 3);
        retval += '-';
        // Add last 4 digits
        retval += line.substr(6);
    }

    return retval;
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
        if (is_mciid(line)) {
            _id = canonicalize_mciid(line);
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

    if (is_mciid(first_part)) {
        // Handle "MCIID / Org or Loc"
        _id = canonicalize_mciid(first_part);
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
        // Deal with "User name / MCIID"
        if (_id.empty()) {
            if (is_mciid(line)) {
                _id = canonicalize_mciid(line);
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
    return std::format("'{}'/'{}'/'{}' b:{}, i:{}, l:{}, o:{}", _name, _id, _organization, _board,
                       _instant, _list, _owner);
}
