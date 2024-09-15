#include <cstring>
#include <exception>
#include <format>
#include <string_view>

#include "mep2_errors.hpp"
#include "mep2_pdu.hpp"
#include "mep2_pdu_parser.hpp"

#include "string_utils.hpp"

void validate_pdu_line(std::string_view line) {
    size_t len = line.length();

    // Shortest possible valid PDU is /ENV\r
    // All PDUs must start with a /
    if (len < 5) {
        throw PduSyntaxError("PDU invalid: too short");
    }

    if (!line.starts_with('/')) {
        throw PduSyntaxError("PDU invalid: doesn't start with a '/'");
    }

    if (std::count(line.cbegin(), line.cend(), '*') > 1) {
        // There can never be more than 1 star
        throw PduSyntaxError("Stray '*' in PDU");
    }

    if (std::count(line.cbegin(), line.cend(), '/') > 1) {
        // There can never be more than 1 star
        throw PduSyntaxError("Stray '/' in PDU");
    }
}

std::string_view strip_pdu_crlf(std::string_view line) {
    size_t pdu_end = line.find_first_of('\r');
    if (pdu_end == std::string_view::npos) {
        throw PduSyntaxError("No carriage return in PDU");
    }

    line = line.substr(0, pdu_end);
    rstrip(line);
    return line;
}

void compare_text_checksum(const PduChecksum& checksum, std::string_view string_checksum) {
    // the "ZZZZ" hash is to be ignored by the server. It is intended for
    // manual testing
    if (icompare(string_checksum, "zzzz")) {
        return;
    }

    try {
        PduChecksum sender_checksum(string_checksum);
        if (checksum != sender_checksum) {
            throw PduChecksumError(std::format("Wanted: {:04X}, actual: {:04X}",
                                               sender_checksum._checksum, checksum._checksum));
        }
    } catch (std::invalid_argument& e) {
        // The sender checksum included invalid characters
        throw PduSyntaxError("Checksum has invalid characters");
    }
}

void PduParser::validate_checksum(std::string_view line) {
#ifdef FUZZING_BUILD
    return;
#endif
    size_t star = line.find_first_of('*');
    if (star == std::string_view::npos) {
        throw PduSyntaxError("PDU line does not have a *");
    }

    // The * must appear here, or there's no space for a checksum
    if (star != line.length() - 5) {
        throw PduSyntaxError("Checksum too short");
    }

    std::string_view pdu_data = line.substr(0, star + 1);
    std::string_view sender_checksum = line.substr(star + 1, 4);

    std::visit(
        [pdu_data, sender_checksum](auto&& pdu) {
            pdu.get_checksum().add_line(pdu_data);
            compare_text_checksum(pdu.get_checksum(), sender_checksum);
        },
        _current_pdu);
}

PduType PduParser::parse_pdu_type(std::string_view& line_parse) {
    auto pdu_type = _pdu_trie.find(line_parse);
    if (!pdu_type) {
        throw PduSyntaxError("Unknown PDU type");
    }

    return *pdu_type;
}

PduType PduParser::parse_pdu_start(std::string_view& line_parse) {
    // Eat leading '/'
    line_parse.remove_prefix(1);
    return parse_pdu_type(line_parse);
}

awaitable<void> PduParser::parse_line(std::string_view line) {
    switch (_state) {
    case state::idle:
        parse_first_line(line);
        break;

    case state::parsing:
        parse_information_line(line);
        break;

    case state::complete:
#ifndef FUZZING_BUILD
        throw PduSyntaxError("Unexpected data after Pdu");
#endif
    }

    co_return;
}

void PduParser::parse_first_line(std::string_view line) {
    // Parses the first line of a PDU, in one of two forms:
    // /<pdu type> [ <options>]*ZZZZ\r\n  For single-line PDUs
    // /<pdu type> [ <options>]\r\n  For multi-line PDUs
    // Options is optional

    validate_pdu_line(line);
    std::string_view line_strip = strip_pdu_crlf(line);
    std::string_view line_parse = line_strip;
    PduType type = parse_pdu_start(line_parse);

    // Eat optional whitespace between pdu type and options or checksum
    lstrip(line_parse);

    switch (type.get_id()) {
    case PduType::type_id::busy:
        _current_pdu.emplace<BusyPdu>();
        break;
    case PduType::type_id::create:
        _current_pdu.emplace<CreatePdu>();
        break;
    case PduType::type_id::term:
        _current_pdu.emplace<TermPdu>();
        break;
    case PduType::type_id::send:
        _current_pdu.emplace<SendPdu>();
        break;
    case PduType::type_id::scan:
        _current_pdu.emplace<ScanPdu>();
        break;
    case PduType::type_id::turn:
        _current_pdu.emplace<TurnPdu>();
        break;
    case PduType::type_id::comment:
        _current_pdu.emplace<CommentPdu>();
        break;
    case PduType::type_id::verify:
        _current_pdu.emplace<VerifyPdu>();
        break;
    case PduType::type_id::env:
        _current_pdu.emplace<EnvPdu>();
        break;
    case PduType::type_id::text:
        _current_pdu.emplace<TextPdu>();
        break;
    default:
        throw PduSyntaxError("Unhandled PDU type");
    }

    _current_type.emplace(type);

    if (type.is_single_line()) {
        validate_checksum(line_strip);
        // Done with the checksum
        line_parse = line_parse.substr(0, line_parse.find("*"));
    } else {
        // Multi-line PDUs should not have a '*' at all on the first line.
        if (line.find("*") != std::string_view::npos) {
            throw PduSyntaxError("Unexpected checksum for multi-line PDU");
        }

        // For a multi-line PDU any trailing whitespace or newlines are
        // part of the checksum
        std::visit([line](auto&& pdu) { pdu.get_checksum().add_line(line); }, _current_pdu);
    }

    // strip any trailing whitespace after the options, this is legal
    rstrip(line_parse);

    // Parse options
    std::visit([line_parse](auto&& pdu) { pdu.parse_options(line_parse); }, _current_pdu);

    if (type.is_single_line()) {
        _state = state::complete;
    } else {
        _state = state::parsing;
    }
}

void PduParser::parse_information_line(std::string_view line) {
    // We want to ensure that we only throw anything if the entire PDU has been
    // parsed

    if (line.length()) {
        if (line[0] == '/') {
            // Complete PDU parsing
            parse_end_line(line);

            // If we have any error codes for the PDU contents, throw them now
            if (_current_error.has_value()) {
                std::rethrow_exception(*_current_error);
            }

            // Let the PDU do a semantic check, if necessary
            std::visit([](auto&& pdu) { pdu.finalize(); }, _current_pdu);
        } else {
            try {
                std::visit(
                    [this, line](auto&& pdu) {
                        pdu.get_checksum().add_line(line);
                    // If we have an error code, we don't want to do any
                    // more parsing
#ifndef FUZZING_BUILD
                        if (!_current_error.has_value()) {
                            pdu.parse_line(line);
                        }
#else
                        // While fuzzing we want to continue on
                        pdu.parse_line(line);
#endif
                    },
                    _current_pdu);
                // Store any errors for later
            } catch (...) {
                _current_error = std::current_exception();
            }
        }
    }
}

void PduParser::parse_end_line(std::string_view line) {
    // Parses the /end pdu in the following form:
    // /end <pdu type>*<checksum>\r

    validate_pdu_line(line);
    std::string_view line_strip = strip_pdu_crlf(line);
    std::string_view line_parse = line_strip;
    PduType type = parse_pdu_start(line_parse);

    if (type.get_id() != PduType::type_id::end) {
        throw PduSyntaxError("Unexpected PDU, expected end");
    }

    validate_checksum(line_strip);

    // Done with the checksum
    line_parse = line_parse.substr(0, line_parse.find("*"));
    // Strip all whitespace between /end and <type>
    lstrip(line_parse);

    std::visit(
        [&line_parse, this](auto&& pdu) {
            // The start of the parser line should now be the type of the
            // end
            PduType end_type = parse_pdu_type(line_parse);
            if (end_type.get_id() != pdu.get_type().get_id()) {
                throw PduSyntaxError(
                    std::format("Unexpected PDU, expected end {}", pdu.get_type().get_name()));
            }
        },
        _current_pdu);

    // There should be no more data left except optional whitespace
    lstrip(line_parse);

#ifndef FUZZING_BUILD
    if (line_parse.length()) {
        throw PduSyntaxError(std::format("Unexpected data after end type: '{}'", line_parse));
    }
#endif

    _state = state::complete;
}
