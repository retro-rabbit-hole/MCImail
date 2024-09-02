#include <string>

#include "date.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size) {
    if (size < 29)
        return 0;

    std::string s;
    s = std::string(data, 29);

    try {
        Date d;
        d.parse(s);
        d.to_utc_string();
        d.to_orig_string();
    } catch (...) {
        return 0;
    }

    return 0;
}