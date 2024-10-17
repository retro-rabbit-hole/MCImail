#include <stdexcept>
#include <string_view>

#include "address.hpp"
#include "mep2_errors.hpp"
#include "mep2_pdu.hpp"
#include "mep2_pdu_parser.hpp"
#include "string_utils.hpp"

void QueryPdu::parse_options(std::string_view options) {
    while (options.length()) {
        std::string_view option;

        size_t delim = options.find_first_of(',');
        if (delim == std::string_view::npos) {
            option = options;
            options.remove_prefix(options.length());
        } else {
            option = options.substr(0, delim);
            options.remove_prefix(delim + 1);
        }

        std::string_view keyword;
        std::string_view value;

        size_t equals = option.find_first_of('=');
        if (equals == std::string_view::npos) {
            keyword = option;
        } else {
            keyword = option.substr(0, equals);
            value = option.substr(equals + 1);

            // The minimal value size is 3 '(x)'
            // while 0 length is valid, that is only true if there was no '='
            if (value.length() <= 3) {
                throw PduSyntaxError("Value length invalid");
            }
        }

        // println("Parsing {} left {}", option, options);

        if (value.length() == 0) {
            if (keyword == "PRIORITY") {
                _priority = true;
                continue;
            } else {
                throw PduSyntaxError("Missing value");
            }
        }

        // Values must be enclosed in a '()'
        if (value.starts_with('(') && value.ends_with(')')) {
            // We don't actually care about the enclosing parenthesis
            value.remove_prefix(1);
            value.remove_suffix(1);
        } else {
            throw PduSyntaxError("Value must be enclosed in parenthesis");
        }

        // There cannot be any ( or ) symbols inside the values
        if (value.find('(') != std::string_view::npos ||
            value.find(')') != std::string_view::npos) {
            throw PduSyntaxError("Value cannot contain parenthesis");
        }

        // println("Key {} value {}", keyword, value);

        if (keyword == "FOLDER") {
            if (value == "OUTBOX") {
                _folder = folder_id::outbox;
            } else if (value == "INBOX") {
                _folder = folder_id::inbox;
            } else if (value == "DESK") {
                _folder = folder_id::desk;
            } else if (value == "TRASH") {
                _folder = folder_id::trash;
            } else {
                throw PduMalformedDataError("Unknown folder type in folder query");
            }
        } else if (keyword == "SUBJECT") {
            try {
                _subject = decode_string(value);
            } catch (std::invalid_argument& e) {
                throw PduMalformedDataError("Invalid %% code in subject query");
            }

            if (!is_printable(_subject)) {
                throw PduMalformedDataError("Invalid characters in subject query");
            }
        } else if (keyword == "FROM") {
            try {
                _from = decode_string(value);
            } catch (std::invalid_argument& e) {
                throw PduMalformedDataError("Invalid %% code in from query");
            }

            if (!is_printable(_from)) {
                throw PduMalformedDataError("Invalid characters in from query");
            }
        } else if (keyword == "MAXSIZE") {
            // Unimplemented
        } else if (keyword == "MINSIZE") {
            // Unimplemented
        } else if (keyword == "BEFORE") {
            // Unimplemented
        } else if (keyword == "AFTER") {
            // Unimplemented
        } else {
            throw PduSyntaxError("Unknown keyword");
        }
    }
}

void CommentPdu::_parse_line(std::string_view line) {
    // We don't actually care about the data, only that it doesn't contain
    // illegal characters
    try {
        std::string line_decoded = decode_string(line);
        std::string_view sv_line_decoded(line_decoded);
        sv_line_decoded = strip_pdu_crlf(sv_line_decoded);
        // We might want to log the decoded comment

    } catch (std::invalid_argument& e) {
        throw PduMalformedDataError(e.what());
    }
}

void EnvelopeHeaderPdu::parse_options(std::string_view options) {
    // This is fine, no priority query
    if (!options.length()) {
        return;
    }

    if (options == "POSTAL") {
        _priority = priority_id::postal;
    } else if (options == "ONITE") {
        _priority = priority_id::onite;
    } else {
        throw PduMalformedDataError("Unknown priority");
    }
}

EnvelopeHeaderPdu::header_field split_envelope_line(std::string_view line, std::string_view& field,
                                                    std::string_view& information) {

    line = strip_pdu_crlf(line);

    if (!line.length()) {
        throw PduMalformedDataError("Empty envelope line");
    }

    if (std::count(line.cbegin(), line.cend(), ':') < 1) {
        throw PduMalformedDataError("Missing : in envelope line");
    }

    EnvelopeHeaderPdu::header_field f;
    size_t colon = line.find_first_of(':');
    field = line.substr(0, colon + 1);
    information = line.substr(colon + 1);

    // we don't care about trailing whitespace, but we do care about leading
    // whitespace as address continuations must start with whitespace
    rstrip(field);
    // We don't care about whitespace at the start or end of an address pair
    strip(information);

    if (icompare(line, "from:")) {
        f = EnvelopeHeaderPdu::header_field::from;
    } else if (icompare(line, "to:")) {
        f = EnvelopeHeaderPdu::header_field::to;
    } else if (icompare(line, "cc:")) {
        f = EnvelopeHeaderPdu::header_field::cc;
    } else if (icompare(line, "date:")) {
        f = EnvelopeHeaderPdu::header_field::date;
    } else if (icompare(line, "source-date:")) {
        f = EnvelopeHeaderPdu::header_field::source_date;
    } else if (icompare(line, "message-id:")) {
        f = EnvelopeHeaderPdu::header_field::message_id;
    } else if (icompare(line, "source-message-id:")) {
        f = EnvelopeHeaderPdu::header_field::source_message_id;
    } else if (icompare(line, "subject:")) {
        f = EnvelopeHeaderPdu::header_field::subject;
    } else if (icompare(line, "handling:")) {
        f = EnvelopeHeaderPdu::header_field::handling;
    } else if (icompare(line, "u-")) {
        f = EnvelopeHeaderPdu::header_field::U;
    } else if (line.starts_with(' ') || line.starts_with('\t')) {
        lstrip(field);
        f = EnvelopeHeaderPdu::header_field::address_cont;
    } else {
        throw PduMalformedDataError("Invalid header type");
    }

    return f;
}

void EnvelopeHeaderPdu::finish_current_address() {
    switch (_address_parse_state) {
    case address_parse_state::idle:
        return;

    case address_parse_state::parsing_to:
        _to_address.push_back(std::move(_current_address));
        break;

    case address_parse_state::parsing_cc:
        _cc_address.push_back(std::move(_current_address));
        break;

    case address_parse_state::parsing_from:
        _from_address = std::move(_current_address);
        break;
    }

    _address_parse_state = address_parse_state::idle;
}

void EnvelopeHeaderPdu::parse_envelope_line(std::string_view line, bool address_only) {
    if (!line.length()) {
        throw PduMalformedDataError("Empty address line");
    }

    std::string_view field;
    std::string_view information;
    header_field type = split_envelope_line(line, field, information);

    std::string information_decoded;
    try {
        information_decoded = decode_string(information);
    } catch (std::invalid_argument& e) {
        throw PduMalformedDataError(e.what());
    }

    if (address_only) {
        switch (type) {
        case header_field::address_cont:
        case header_field::to:
        case header_field::cc:
            break;
        default:
            throw PduMalformedDataError("Invalid addressing type");
        }
    }

    // If we have are parsing an address save it now.
    if (type != header_field::address_cont) {
        finish_current_address();
    }

    switch (type) {
        // We only accept ems and mbx lines as part of an address
    case header_field::address_cont: {
        if (_address_parse_state == address_parse_state::idle) {
            throw PduMalformedDataError("Invalid start of address");
        }

        // Only throw this error if everything else appears okay
        if (!is_printable(information_decoded)) {
            throw PduMalformedDataError("Invalid characters in address");
        }

        _current_address.parse_field(field, information_decoded);
        break;
    }
        // A To: or Cc: is the start of a new address
    case header_field::to:
    case header_field::cc:
    case header_field::from: {
        switch (type) {
        case header_field::to:
            _address_parse_state = address_parse_state::parsing_to;
            break;
        case header_field::cc:
            _address_parse_state = address_parse_state::parsing_cc;
            break;
        case header_field::from:
            if (_from_address.has_value()) {
                throw PduEnvelopeDataError("Multiple FROM: addresses");
            }
            _address_parse_state = address_parse_state::parsing_from;
            break;
        default:
            throw UnableToPerformError("Unknown error parsing envelope data");
        }

        // Only throw this error if everything else appears okay
        if (!is_printable(information_decoded)) {
            throw PduMalformedDataError("Invalid characters in address");
        }

        _current_address.parse_first_line(information_decoded);
        break;
    }

    case header_field::date:
    case header_field::source_date: {
        Date d;
        d.parse(information_decoded);
        if (type == header_field::date)
            _date.emplace(std::move(d));
        if (type == header_field::source_date)
            _source_date.emplace(std::move(d));
        break;
    }

    case header_field::subject: {
        _subject = std::string(information_decoded.substr(0, 255));
        break;
    }

    case header_field::message_id: {
        _message_id = std::string(information_decoded.substr(0, 100));
        break;
    }

    case header_field::source_message_id: {
        if (_source_message_id.size() == 5) {
            _source_message_id.erase(_source_message_id.begin());
        }
        _source_message_id.push_back(std::string(information_decoded.substr(0, 78)));
        break;
    }

    case header_field::U: {
        if (_u_fields.size() == 5) {
            _u_fields.erase(_u_fields.begin());
        }

        // remove ":"
        field.remove_suffix(1);
        _u_fields.push_back(
            {std::string(field.substr(0, 20)), std::string(information_decoded.substr(0, 78))});
        break;
    }

    case header_field::handling: {
        break;
    }
    }

    // We saw *something* valid
    _envelope_data = true;
}

void EnvelopeHeaderPdu::_finalize() {
    finish_current_address();

#ifndef FUZZING_BUILD
    if (!_envelope_data) {
        throw PduNoEnvelopeDataError();
    }

    if (_to_address.empty()) {
        throw PduToRequiredError();
    }
#endif
}

void TextPdu::parse_options(std::string_view options) {
    // This is fine, default to ascii
    if (!options.length()) {
        return;
    }

    // Parse type field
    lstrip(options);
    if (icompare(options, "ascii")) {
        _content_type = content_type::ascii;
        _content_type_handling = content_type::ascii;
    } else if (icompare(options, "printable")) {
        _content_type = content_type::printable;
        _content_type_handling = content_type::ascii;
    } else if (icompare(options, "env")) {
        _content_type = content_type::env;
        _content_type_handling = content_type::env;
    } else if (icompare(options, "binary")) {
        _content_type = content_type::binary;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "g3fax")) {
        _content_type = content_type::g3fax;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "tlx")) {
        _content_type = content_type::tlx;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "voice")) {
        _content_type = content_type::voice;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "tif0")) {
        _content_type = content_type::tif0;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "tif1")) {
        _content_type = content_type::tif1;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "ttx")) {
        _content_type = content_type::ttx;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "videotex")) {
        _content_type = content_type::videotex;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "encrypted")) {
        _content_type = content_type::encypted;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "sfd")) {
        _content_type = content_type::sfd;
        _content_type_handling = content_type::binary;
    } else if (icompare(options, "racal")) {
        _content_type = content_type::racal;
        _content_type_handling = content_type::binary;
    } else {
        throw PduMalformedDataError("Unknown text type");
    }

    // Parse description
    size_t delim = options.find_first_of(':');
    if (delim == std::string_view::npos) {
        return;
    }

    if (delim == options.length()) {
        return;
    }

    std::string_view description = options.substr(delim + 1);
    strip(description);

    if (!description.length()) {
        return;
    }

    _description = decode_string(description);
}

void TextPdu::_parse_line(std::string_view line) {}
