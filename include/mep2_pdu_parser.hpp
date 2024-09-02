#ifndef INCLUDE_MEP2_PDU_PARSER_HPP_
#define INCLUDE_MEP2_PDU_PARSER_HPP_

#include <stdexcept>
#include <string_view>

#include "mep2_pdu.hpp"
#include "trie.hpp"

void validate_checksum(const PduChecksum& checksum, std::string_view line);
std::string_view strip_pdu_crlf(std::string_view line);

consteval auto create_pdu_trie() {
    Trie<PduType::type_id, 15, 7> trie;
    trie.insert("busy", PduType::type_id::busy);
    trie.insert("comment", PduType::type_id::comment);
    trie.insert("create", PduType::type_id::create);
    trie.insert("end", PduType::type_id::end);
    trie.insert("env", PduType::type_id::env);
    trie.insert("hdr", PduType::type_id::hdr);
    trie.insert("init", PduType::type_id::init);
    trie.insert("reply", PduType::type_id::reply);
    trie.insert("reset", PduType::type_id::reset);
    trie.insert("scan", PduType::type_id::scan);
    trie.insert("send", PduType::type_id::send);
    trie.insert("term", PduType::type_id::term);
    trie.insert("text", PduType::type_id::text);
    trie.insert("turn", PduType::type_id::turn);
    trie.insert("verify", PduType::type_id::verify);
    return trie;
}

class PduParser {
  public:
    void parse_line(std::string_view line);

    PduVariant extract_pdu() {
        if (_state != state::complete) {
            throw std::runtime_error("extract_pdu called in invalid state");
        }

        reset();
        return std::move(_current_pdu);
    }

    const std::optional<PduType> get_current_type() const { return _current_type; }

    bool is_complete() const { return _state == state::complete; }
    bool has_error() const { return _current_error.has_value(); }

    void reset() {
        _state = state::idle;
        _current_type.reset();
        _current_error.reset();
    }

  private:
    PduType parse_pdu_start(std::string_view& line_parse);
    PduType parse_pdu_type(std::string_view& line_parse);

    void parse_first_line(std::string_view line);
    void parse_information_line(std::string_view line);
    void parse_end_line(std::string_view line);
    void validate_checksum(std::string_view line);

    enum class state { idle, parsing, complete };

    state _state = state::idle;
    std::optional<PduType> _current_type;
    std::optional<std::exception_ptr> _current_error;

    PduVariant _current_pdu;

    static constexpr auto _pdu_trie = create_pdu_trie();
};

#endif /* INCLUDE_MEP2_PDU_PARSER_HPP_ */
