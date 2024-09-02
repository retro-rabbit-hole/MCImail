#include <string>

#include "string_utils.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size) {
    std::string s(data, size);

    try {
        decode_string(s);
    } catch (...) {
        return 0;
    }

    return 0;
}