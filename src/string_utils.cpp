#include <stdexcept>
#include <string>

#include "string_utils.hpp"

#define TAB_WIDTH 4

inline void tab_fill(std::string& out) {
    size_t next_tab = TAB_WIDTH - (out.length() % TAB_WIDTH);
    for (size_t t = 0; t < next_tab; ++t) {
        out.append(" ");
    }
}

inline unsigned char decode_percent(std::string_view sv) {
    if (sv.length() != 3) {
        throw std::invalid_argument("Invalid %% code");
    }

    if (sv[0] != '%') {
        throw std::runtime_error("expected %% code");
    }
	
    unsigned char hex1 = sv[1];
    unsigned char hex2 = sv[2];
    return (hex_to_char(hex1) << 4) | hex_to_char(hex2);
}

std::string decode_string(std::string_view sv) {
    std::string result;
    result.reserve(sv.length());

    /* Decoding has to occur in two phases:
    1) Interpret single byte values
    2) If we decoded a % value, interpret that byte byte too.
       These are similar, but not exactly the same.
    */

    for (size_t i = 0; i < sv.length(); ++i) {
        unsigned char c = sv[i];

        // When receiving data through mep2 we discard the top bits
        c &= 0x7F;

        // It is always illegal for a / to appear unescaped
        if (c == '/') {
            throw std::invalid_argument("Stray / in data");
        }

        // % decode
        if (c == '%') {
            if (i + 2 >= sv.length()) {
                throw std::invalid_argument("Invalid %% code: too little space");
            }
            
            // transparent %\r\n, this is not actually part of the text
            if (sv[i + 1] == '\r' && sv[i + 2] == '\n') {
				i += 2;
				continue;
			}
			
            c = decode_percent(sv.substr(i, 3));
            i += 2;

            // Also strip top bits when decoding
            c &= 0x7F;

            // We only accept carriage return or linefeed as part of a \r\n
            // sequence
            if (c == 0x0D) {
                if (i + 2 >= sv.length()) {
                    if (sv[i] == '%') {
                        unsigned char c_lf = decode_percent(sv.substr(i, 3));
                        c_lf &= 0x7F;

                        if (c_lf == 0x0A) {
                            result.append("\r\n");
                            i += 2;
                        }
                    }
                }

                continue;
            }

            // These get dropped even when decoding
            if (c == 0x0A || c == 0x0B || c == 0x0C) {
                continue;
            }

            result.push_back(c);
            continue;
        }

        switch (c) {
            // Tab fill to spaces
        case 0x09:
            tab_fill(result);
            continue;

            // We only accept carriage return or linefeed as part of a \r\n
            // sequence
        case 0x0D: {
            if (i + 1 < sv.length()) {
                if (sv[i + 1] == 0x0A) {
                    result.append("\r\n");
                    ++i;
                }
            }
            continue;
        }

            // These values just get lost
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0F:
        case 0x11:
        case 0x12:
        case 0x13:
            continue;

            // The following discard all data up to and including this
            // character
        case 0x15:
        case 0x18:
            result.clear();
            continue;

            // Delete
        case 0x7F:
            if (!result.empty())
                result.pop_back();
            continue;

        default:
            result.push_back(c);
        }
    }

    return result;
}

std::string encode_string(std::string_view input) {
    std::string result;
    result.reserve(input.length() * 3); // Reserve space for worst-case scenario

    // Characters that need to be encoded
    static const std::array<unsigned char, 9> special_chars = {0x00, 0x0F, 0x11, 0x12, 0x13,
                                                               0x15, 0x18, '%',  '/'};

    auto encode_char = [&](unsigned char c) {
        if (c & 0x80 ||
            std::find(special_chars.begin(), special_chars.end(), c) != special_chars.end()) {
            result += '%';
            result += char_to_hex((c >> 4) & 0x0F);
            result += char_to_hex(c & 0x0F);
        } else {
            result += c;
        }
    };

    size_t chars_since_cr = 0;

    for (char c : input) {
        if (c == '\r') {
            encode_char(c);
            chars_since_cr = 0;
        } else {
            if (chars_since_cr >= 200) {
                result += "%\r\n";
                chars_since_cr = 0;
            }
            encode_char(c);
            ++chars_since_cr;
        }
    }

    return result;
}