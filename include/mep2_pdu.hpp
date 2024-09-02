#ifndef INCLUDE_MEP2_PDU_HPP_
#define INCLUDE_MEP2_PDU_HPP_

#include <print>

#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string_view>
#include <variant>

#include "address.hpp"
#include "date.hpp"
#include "mep2_errors.hpp"
#include "string_utils.hpp"

class PduType {
  public:
    enum class type_id {
        busy = 0,
        comment,
        create,
        end,
        env,
        hdr,
        init,
        reply,
        reset,
        scan,
        send,
        term,
        text,
        turn,
        verify
    };

    constexpr bool is_single_line() const {
        switch (_type) {
        case type_id::create:
        case type_id::send:
        case type_id::scan:
        case type_id::busy:
        case type_id::turn:
        case type_id::term:
            return true;
        default:
            return false;
        }
    }

    constexpr bool has_options() const {
        switch (_type) {
        case type_id::verify:
        case type_id::text:
        case type_id::scan:
        case type_id::turn:
        case type_id::reply:
            return true;
        default:
            return false;
        }
    }

    constexpr PduType(type_id type) : _type(type) {}
    constexpr type_id get_id() const { return _type; }
    constexpr const std::string_view get_name() const { return _name[static_cast<int>(_type)]; }

    constexpr operator int() const { return static_cast<int>(_type); }
    constexpr operator const std::string_view() const { return get_name(); }
    constexpr operator const char*() const { return get_name().data(); }

  private:
    static constexpr std::array<std::string_view, 15> _name = {
        "BUSY",  "COMMENT", "CREATE", "END",  "ENV",  "HDR",  "INIT",  "REPLY",
        "RESET", "SCAN",    "SEND",   "TERM", "TEXT", "TURN", "VERIFY"};
    const type_id _type;
};

struct PduChecksum {
    PduChecksum(std::string_view checksum) {
        if (checksum.length() != 4) {
            throw std::invalid_argument("Input must be exactly 4 characters long");
        }

        for (auto c : checksum) {
            _checksum <<= 4;
            _checksum |= char_to_hex(c);
        }
    }
    PduChecksum(uint16_t checksum) : _checksum(checksum) {}
    PduChecksum() = default;

    operator uint16_t() const { return _checksum; }
    const std::string to_string() const { return std::format("{:04X}", _checksum); }

    void add_line(const std::string_view line) {
        // std::println("Adding line: '{}'", line);
        for (auto c : line) {
            // The upper bits should never appear, but if they
            // somehow do we must ignore them
            _checksum += c & 0x7F;
        }
    }

    uint16_t _checksum = 0;
};

class Pdu {
  public:
    Pdu(PduType type) : _type(type) {};
    PduChecksum& get_checksum() { return _checksum; }
    const PduType get_type() const { return _type; }

    void parse_line(std::string_view line) {
        if (_type.is_single_line()) {
            throw PduSyntaxError("Parse line called on single-line PDU");
        }

        _parse_line(line);
    };

    void finalize() {
        if (_type.is_single_line()) {
            throw PduSyntaxError("Finalize calledd single-line PDU");
        }

        _finalize();
    };

    virtual void parse_options(std::string_view options) {
        if (options.length()) {
            throw PduSyntaxError("Option for non-option PDU");
        }
    };

    virtual ~Pdu() = default;

  protected:
    virtual void _parse_line([[maybe_unused]] std::string_view line) {
        throw std::runtime_error("Pdu::_parse_line() base called without implementation");
    };

    virtual void _finalize() {
        throw std::runtime_error("Pdu::_finalize() base called without implementation");
    };

  private:
    PduChecksum _checksum;
    const PduType _type;
};

class BusyPdu : public Pdu {
  public:
    BusyPdu() : Pdu(PduType(PduType::type_id::busy)) {}
};

class CreatePdu : public Pdu {
  public:
    CreatePdu() : Pdu(PduType(PduType::type_id::create)) {}
};

class TermPdu : public Pdu {
  public:
    TermPdu() : Pdu(PduType(PduType::type_id::term)) {}
};

class SendPdu : public Pdu {
  public:
    SendPdu() : Pdu(PduType(PduType::type_id::send)) {}
};

class QueryPdu : public Pdu {
  public:
    enum class folder_id { outbox, inbox, desk, trash };

    void parse_options(std::string_view options);

    folder_id get_folder_id() const { return _folder; }
    const std::string& get_subject() const { return _subject; }
    const std::string& get_from() const { return _from; }

  protected:
    QueryPdu(PduType type) : Pdu(type) {}

  private:
    folder_id _folder = folder_id::inbox;
    std::string _subject;
    std::string _from;

    bool _priority = false;
};

class ScanPdu : public QueryPdu {
  public:
    ScanPdu() : QueryPdu(PduType(PduType::type_id::scan)) {}
};

class TurnPdu : public QueryPdu {
  public:
    TurnPdu() : QueryPdu(PduType(PduType::type_id::turn)) {}
};

class CommentPdu : public Pdu {
  public:
    CommentPdu() : Pdu(PduType(PduType::type_id::comment)) {}

  private:
    void _parse_line(std::string_view line);
    // Nothing to do
    void _finalize() {};
};

class EnvelopeHeaderPdu : public Pdu {
  public:
    enum class priority_id { none, postal, onite };
    enum class header_field {
        from,
        to,
        cc,
        date,
        source_date,
        message_id,
        source_message_id,
        subject,
        handling,
        U,
        address_cont,
    };

    void parse_options(std::string_view options);
    priority_id get_priority_id() const { return _priority; }

    const std::vector<RawAddress>& get_to_address() const { return _to_address; }

    const std::vector<RawAddress>& get_cc_address() const { return _cc_address; }

  protected:
    EnvelopeHeaderPdu(PduType type) : Pdu(type) {}
    void parse_envelope_line(std::string_view line, bool address_only);
    void _finalize();

    void finish_current_address();

    enum class address_parse_state { idle, parsing_to, parsing_cc, parsing_from };

    bool _envelope_data = false;
    address_parse_state _address_parse_state = address_parse_state::idle;

    priority_id _priority = priority_id::none;
    RawAddress _current_address;
    std::optional<RawAddress> _from_address;
    std::vector<RawAddress> _to_address;
    std::vector<RawAddress> _cc_address;

    std::optional<Date> _date;
    std::optional<Date> _source_date;
    std::optional<std::string> _subject;
    std::optional<std::string> _message_id;
    std::vector<std::string> _source_message_id;
    std::vector<std::pair<std::string, std::string>> _u_fields;
};

class VerifyPdu : public EnvelopeHeaderPdu {
  public:
    VerifyPdu() : EnvelopeHeaderPdu(PduType(PduType::type_id::verify)) {}

  protected:
    void _parse_line(std::string_view line) { parse_envelope_line(line, true); }
};

class EnvPdu : public EnvelopeHeaderPdu {
  public:
    EnvPdu() : EnvelopeHeaderPdu(PduType(PduType::type_id::env)) {}

    const RawAddress& get_from_address() const { return *_from_address; }
    const Date& get_date() const { return *_date; }
    const Date& get_source_date() const { return *_source_date; }
    const std::string& get_subject() const { return *_subject; }
    const std::string& get_message_id() const { return *_message_id; }
    const std::vector<std::string>& get_source_message_id() const { return _source_message_id; }
    const std::vector<std::pair<std::string, std::string>>& get_u_fields() const {
        return _u_fields;
    }

    bool has_from_address() const { return _from_address.has_value(); }
    bool has_date() const { return _date.has_value(); }
    bool has_source_date() const { return _source_date.has_value(); }
    bool has_subject() const { return _subject.has_value(); }
    bool has_message_id() const { return _message_id.has_value(); }
    bool has_source_message_id() const { return !_source_message_id.empty(); }
    bool has_u_fields() const { return !_u_fields.empty(); }

  protected:
    void _parse_line(std::string_view line) { parse_envelope_line(line, false); }
};

using PduVariant = std::variant<BusyPdu, CreatePdu, TermPdu, SendPdu, ScanPdu, TurnPdu, CommentPdu,
                                VerifyPdu, EnvPdu>;

#endif /* INCLUDE_MEP2_PDU_HPP_ */
