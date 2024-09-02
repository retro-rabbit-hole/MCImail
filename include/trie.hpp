#ifndef INCLUDE_TRIE_HPP_
#define INCLUDE_TRIE_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

template <typename CommandEnum, size_t MaxCommands, size_t MaxDepth> class Trie {
  private:
    static constexpr size_t alphabet_size = 26;

    struct Node {
        std::array<int16_t, alphabet_size> children;
        CommandEnum cmd;
        bool is_end;

        consteval Node() : children{}, cmd{}, is_end(false) { children.fill(-1); }
    };

    std::array<Node, MaxCommands * MaxDepth> nodes;
    int16_t node_count;

    static constexpr char lower(const char c) {
        return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
    }

    static constexpr bool is_valid_command_char(const char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

  public:
    consteval Trie() : nodes{}, node_count(1) {}

    consteval void insert(const std::string_view str, const CommandEnum cmd) {
        int16_t node = 0;
        for (char ch : str) {
            if (!is_valid_command_char(ch)) {
                throw("Command names can only consist of a-zA-Z");
            }

            uint8_t index = lower(ch) - 'a';
            if (nodes[node].children[index] == -1) {
                nodes[node].children[index] = node_count++;
            }
            node = nodes[node].children[index];
        }
        nodes[node].is_end = true;
        nodes[node].cmd = cmd;
    }

    constexpr std::optional<CommandEnum> find(std::string_view& str) const {
        int16_t node = 0;
        uint32_t consumed = 0;

        for (char ch : str) {
            if (!is_valid_command_char(ch))
                break;

            ++consumed;

            uint8_t index = lower(ch) - 'a';
            if (nodes[node].children[index] == -1) {
                return std::nullopt;
            }
            node = nodes[node].children[index];
        }

        if (nodes[node].is_end) {
            str.remove_prefix(consumed);
            return std::optional(nodes[node].cmd);
        }

        return std::nullopt;
    }
};

#endif /* INCLUDE_TRIE_HPP_ */
