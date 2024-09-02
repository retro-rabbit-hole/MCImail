#include <string_view>

#include "mep2_pdu_parser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size) {
    PduParser p;

    std::string_view sv(data, size);
    while (!sv.empty()) {
        size_t l = sv.find_first_of("\r\n");
        std::string_view line = sv.substr(0, l + 2);
        if (l == std::string_view::npos)
            break;
        try {
            // std::println("Parsing: {}", sv.substr(0, l + 1));
            p.parse_line(line);
        } catch (...) {
            return 0;
        }

        sv.remove_prefix(line.length());
    }
    return 0;
}