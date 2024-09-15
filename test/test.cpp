#include <exception>
#include <filesystem>
#include <fstream>
#include <print>
#include <regex>
#include <string_view>
#include <variant>

#include <gtest/gtest.h>

#include "address.hpp"
#include "asio/awaitable.hpp"
#include "date.hpp"
#include "mep2_errors.hpp"
#include "mep2_pdu.hpp"
#include "mep2_pdu_parser.hpp"
#include "string_utils.hpp"
#include "temporary_storage.hpp"

#define CONCAT_IMPL(x, y) x##_##y
#define MACRO_CONCAT(x, y) CONCAT_IMPL(x, y)

std::string escape_crlf(std::string_view s) {
    std::string res;
    res = std::regex_replace(std::string(s), std::regex("\r"), "\\r");
    res = std::regex_replace(res, std::regex("\n"), "\\n");
    return res;
}

class AsyncTestBase {
  protected:
    asio::io_context io_context;

    template <typename Func> void RunAsync(Func&& func) {
        std::exception_ptr eptr;
        io_context.reset();

        auto completion_handler = [&](std::exception_ptr ep) {
            if (ep) {
                eptr = ep;
            }
        };

        co_spawn(io_context, std::forward<Func>(func), completion_handler);

        io_context.run();

        if (eptr) {
            std::rethrow_exception(eptr);
        }
    }
};

class PduParserTestBase : public AsyncTestBase {
  protected:
    PduParser p;

    void ParseLine(const std::string& s) {
        RunAsync([&]() -> asio::awaitable<void> {
            p.reset();
            std::string_view sv(s);
            if (sv.empty()) {
                co_await p.parse_line(sv);
                co_return;
            }

            while (!sv.empty()) {
                int delim_len = 1;
                size_t l = sv.find_first_of('\r');

                if (l == std::string_view::npos)
                    break;

                if (l + 1 < sv.length()) {
                    if (sv[l + 1] == '\n') {
                        ++delim_len;
                    }
                }

                co_await p.parse_line(sv.substr(0, l + delim_len));
                sv.remove_prefix(l + delim_len);
            }

            if (!sv.empty()) {
                co_await p.parse_line(sv);
            }
        });
    }
};

class AsyncTest : public AsyncTestBase, public ::testing::Test {};
class PduParserTest : public PduParserTestBase, public ::testing::Test {};

class StringUtilsFixture : public ::testing::Test {
  protected:
    void ExpectLstrip(const std::string& in, const std::string& expected) {
        std::string_view sv(in);
        lstrip(sv);
        EXPECT_EQ(sv, expected);
    }

    void ExpectRstrip(const std::string& in, const std::string& expected) {
        std::string_view sv(in);
        rstrip(sv);
        EXPECT_EQ(sv, expected);
    }

    void ExpectStrip(const std::string& in, const std::string& expected) {
        std::string_view sv(in);
        strip(sv);
        EXPECT_EQ(sv, expected);
    }
};

TEST_F(StringUtilsFixture, lstrip) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        // Comment to make clang-format happy
        {"", ""},
        {" ", ""},
        {"\t \t ", ""},
        {"ABCD", "ABCD"},
        {"ABCD ", "ABCD "},
        {" ABCD", "ABCD"},
        {" ABCD ", "ABCD "},
        {"\tABCD\t", "ABCD\t"},
        {"\t \tAB CD", "AB CD"},
        {"\tA\tB", "A\tB"}};

    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data['" << t.first << "']");
        ExpectLstrip(t.first, t.second);
    }
}

TEST_F(StringUtilsFixture, rstrip) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        // Comment to make clang-format happy
        {"", ""},
        {" ", ""},
        {"\t \t ", ""},
        {"ABCD", "ABCD"},
        {"ABCD ", "ABCD"},
        {" ABCD", " ABCD"},
        {" ABCD ", " ABCD"},
        {"\tABCD\t", "\tABCD"},
        {"\t \tAB CD", "\t \tAB CD"},
        {"\tA\tB", "\tA\tB"}};

    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data['" << t.first << "']");
        ExpectRstrip(t.first, t.second);
    }
}

TEST_F(StringUtilsFixture, strip) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        // Comment to make clang-format happy
        {"", ""},
        {" ", ""},
        {"\t \t ", ""},
        {"ABCD", "ABCD"},
        {"ABCD ", "ABCD"},
        {" ABCD", "ABCD"},
        {" ABCD ", "ABCD"},
        {"\tABCD\t", "ABCD"},
        {"\t \tAB CD", "AB CD"},
        {"\tA\tB", "A\tB"}};

    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data['" << t.first << "']");
        ExpectStrip(t.first, t.second);
    }
}

class PduTypeFixture : public ::testing::Test {
  protected:
    void ExpectCorrectIdName(const PduType::type_id type, const std::string& expectedName) {
        PduType p(type);
        EXPECT_EQ(p.get_id(), type);
        EXPECT_EQ(p.get_name(), expectedName);
    }
};

TEST_F(PduTypeFixture, Types) {
    const std::vector<std::pair<PduType::type_id, std::string>> types = {
        {PduType::type_id::busy, "BUSY"},     {PduType::type_id::comment, "COMMENT"},
        {PduType::type_id::create, "CREATE"}, {PduType::type_id::end, "END"},
        {PduType::type_id::env, "ENV"},       {PduType::type_id::hdr, "HDR"},
        {PduType::type_id::init, "INIT"},     {PduType::type_id::reply, "REPLY"},
        {PduType::type_id::reset, "RESET"},   {PduType::type_id::scan, "SCAN"},
        {PduType::type_id::send, "SEND"},     {PduType::type_id::term, "TERM"},
        {PduType::type_id::text, "TEXT"},     {PduType::type_id::turn, "TURN"},
        {PduType::type_id::verify, "VERIFY"},
    };

    for (const auto& t : types) {
        SCOPED_TRACE(testing::Message() << "with test_data[" << t.second << "]");
        ExpectCorrectIdName(t.first, t.second);
    }
}

TEST(PDUHash, invalid) {
    EXPECT_THROW(PduChecksum("AABBCCDDEEFF"), std::invalid_argument);
    EXPECT_THROW(PduChecksum("ZZZZZZZZZZZZ"), std::invalid_argument);
    EXPECT_THROW(PduChecksum("ZZZZ"), std::invalid_argument);
}

TEST(PDUHash, valid) {
    EXPECT_EQ(PduChecksum("0000")._checksum, 0);
    EXPECT_EQ(PduChecksum("aaaa")._checksum, 43690);
    EXPECT_EQ(PduChecksum("AAAA")._checksum, 43690);
    EXPECT_EQ(PduChecksum("FFFF")._checksum, 65535);
    {
        PduChecksum p;
        p.add_line("/TURN*");
        EXPECT_EQ(p._checksum, 0x01A2);
    }
    {
        PduChecksum p;
        p.add_line("/REPLY SCAN 100\r\n");
        p.add_line("Request performed successfully\r\n");
        p.add_line("POSTED       FROM               SUBJECT                    "
                   " SIZE\r\n");
        p.add_line("Oct 30 15:09 Eileen Gamache     (Forwarded) CPR Training   "
                   "  1345\r\n");
        p.add_line("Oct 31 09:56 Barbara Deniston   (Forwarded) Springs Trek   "
                   "  2664\r\n");
        p.add_line("Oct 31 16:25 Eileen Gamache     Weekly Status Report       "
                   " 30435\r\n");
        p.add_line("Nov 01 08:32 Dan O'Reilly       FYI - ethernet testing     "
                   "   660\r\n");
        p.add_line("Nov 01 11:58 John Weaver        Organizational Change%2FEn "
                   "     869\r\n");
        p.add_line("Nov 04 09:18 Eileen Gamache     Pencil Sharpener           "
                   "   227\r\n");
        p.add_line("/END REPLY*");
        EXPECT_EQ(p._checksum, 0x8CF2);
    }
}

TEST(PDUHash, conversions) {
    EXPECT_EQ(PduChecksum("0000").to_string(), std::string("0000"));
    EXPECT_EQ(PduChecksum("aaaa").to_string(), std::string("AAAA"));
    EXPECT_EQ(PduChecksum("AAAA").to_string(), std::string("AAAA"));
}

class StringDecodeFixture : public ::testing::Test {
  protected:
    void ExpectDecode(const std::string& in, const std::string& expected) {
        EXPECT_EQ(decode_string(in), expected);
    }
};

TEST_F(StringDecodeFixture, ValidValues) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"Simple ASCII string", "Simple ASCII string"},
        {"Simple ASCII string with newline\r\n", "Simple ASCII string with newline\r\n"},
        {"Percent sign %25", "Percent sign %"},
        {"MCI Address: Gandalf%2F111-1111", "MCI Address: Gandalf/111-1111"},
        {"Lost characters\x0b\x0c\x11\x12\x13", "Lost characters"},
        {"Delete characterX\x7f", "Delete character"},
        {"Tab fill\x09tab", "Tab fill    tab"},
        {"Tab fill2\x09tabby\x09tabby\x09tab", "Tab fill2   tabby   tabby   tab"},
        {"This will be entirely deleted\x15Not this", "Not this"},
        {"Single linefeed will be deleted\n", "Single linefeed will be deleted"},
        {"Single carriage return will be deleted\r", "Single carriage return will be deleted"},
        {"Single linefeed will be deleted\x0a", "Single linefeed will be deleted"},
        {"Single carriage return will be deleted\x0d", "Single carriage return will be deleted"},
        {"Strip top bits: \xc1\xd3\xc3\xc9\xc9", "Strip top bits: ASCII"},
        {"Transparent%\r\n crlf are removed", "Transparent crlf are removed"}};

    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data[" << t.first << "]");
        ExpectDecode(t.first, t.second);
    }
}

TEST(StringDecode, invalid) {
    EXPECT_THROW(decode_string("Invalid % code"), std::invalid_argument);
    EXPECT_THROW(decode_string("Invalid percent code %a"), std::invalid_argument);
    EXPECT_THROW(decode_string("Stray / in data"), std::invalid_argument);
}

TEST(is_mciid, invalid) {
    EXPECT_FALSE(is_mciid(""));
    EXPECT_FALSE(is_mciid("111-111-"));
    EXPECT_FALSE(is_mciid("111-111-111"));
    EXPECT_FALSE(is_mciid("111-1111111"));
    EXPECT_FALSE(is_mciid("111--1111111"));
    EXPECT_FALSE(is_mciid("1111-111"));
    EXPECT_FALSE(is_mciid("NOT-REAL"));
    EXPECT_FALSE(is_mciid("NOT-VAL-IDSE"));
}

TEST(is_mciid, valid) {
    EXPECT_TRUE(is_mciid("111-1111"));
    EXPECT_TRUE(is_mciid("111-111-1111"));
    EXPECT_TRUE(is_mciid("000-111-1111"));
    EXPECT_TRUE(is_mciid("0001111111"));
    EXPECT_TRUE(is_mciid("1111111"));
    EXPECT_TRUE(is_mciid("1111111111"));
}

class RawAddressFixture : public ::testing::Test {
  protected:
    void FirstLineExpectEqual(const std::string& line, const RawAddress& expected) {
        RawAddress a;
        a.parse_first_line(line);
        EXPECT_EQ(a._id, expected._id);
    }

    void FirstLineExpectMalformed(const std::string& line) {
        RawAddress a;
        EXPECT_THROW(a.parse_first_line(line), PduMalformedDataError);
    }

    void SecondLineExpectMalformed(const std::string& field, const std::string& information) {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        EXPECT_THROW(a.parse_field(field, information), PduMalformedDataError);
    }
};

TEST_F(RawAddressFixture, FirstLineThrows) {
    const std::vector<std::string> cases = {
        "",
        "NAME/",
        "NAME/ORG/",
        "NAME/ORG/LOC/",
        "NAME/Org:/Loc:",
        "NAME/Org:ORG/Loc:",
        "NAME/Org:/Loc:LOC",
        "NAME/Org:org/Loc:loc/",
        "111-1111/222-2222",
        "NAME/222-2222/stuff",
        "NAME/stuff/222-2222",
        "///",
        "       /     /     /      ",
        "NAME (CRAP)",
        "NAME (BOARD,)",
        "NAME (BOARD,,PRINT)",
        "NAME (,)",
        "NAME (,BOARD)",
    };

    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data[" << t << "]");
        FirstLineExpectMalformed(t);
    }
}

TEST_F(RawAddressFixture, SecondLineThrows) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"ems", ""},
        {"MBX", "lama"},

    };

    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data[" << t.first << ":" << t.second << "]");
        SecondLineExpectMalformed(t.first, t.second);
    }
}

TEST_F(RawAddressFixture, Equals) {
    const std::vector<std::pair<std::string, RawAddress>> cases = {
        // clang-format off
        {"111-1111 ", {._id = "111-1111"}},
        {"1111111 ", {._id = "111-1111"}},
        {"0001111111 ", {._id = "111-1111"}},
        {"000-111-1111 ", {._id = "111-1111"}},
        {"000-1111 ", {._id = "000-1111"}},
        {"111-111-1111 ", {._id = "111-111-1111"}},
        {"1111111111 ", {._id = "111-111-1111"}},
        {"0011111111 ", {._id = "001-111-1111"}},
        {"MCI ID: 111-1111", {._id = "111-1111"}},
        {"Gandalf the Gray / MCI ID: 111-1111", {._name = "Gandalf the Gray", ._id = "111-1111"}},
        {"Gandalf the Gray  ", {._name = "Gandalf the Gray"}},
        {"Gandalf the Gray/111-1111", {._name = "Gandalf the Gray", ._id = "111-1111"}},
        {"Gandalf the Gray / 111-1111 ", { ._name = "Gandalf the Gray", ._id = "111-1111", }}, 
        {"Gandalf the Gray / Org: The Good Guys ", {._name = "Gandalf the Gray", ._organization = "The Good Guys"}},
        {"Gandalf the Gray / Org: The Good Guys / Loc: Hobbiton ", {._name = "Gandalf the Gray", ._organization = "The Good Guys", ._location = "Hobbiton"}},
        {"Gandalf the Gray / The Good Guys / Loc: Hobbiton ", {._name = "Gandalf the Gray", ._location = "Hobbiton", ._unresolved_org_loc_1 = "The Good Guys"}},
        {"Gandalf the Gray / Org: The Good Guys / Hobbiton ", {._name = "Gandalf the Gray", ._organization = "The Good Guys", ._unresolved_org_loc_1 = "Hobbiton"}},
        {"Gandalf the Gray / The Good Guys / Hobbiton ", {._name = "Gandalf the Gray", ._unresolved_org_loc_1 = "The Good Guys", ._unresolved_org_loc_2 = "Hobbiton"}},
        {"Gandalf the Gray ( BOARD )", {._name = "Gandalf the Gray", ._board = true}},
        {"Gandalf the Gray (       BOARD)", {._name = "Gandalf the Gray", ._board = true}},
        {"Gandalf the Gray (BOARD)", {._name = "Gandalf the Gray", ._board = true}},
        {"Gandalf the Gray (INSTANT)", {._name = "Gandalf the Gray", ._instant = true}},
        {"Gandalf the Gray (LIST)", {._name = "Gandalf the Gray", ._list = true}},
        {"Gandalf the Gray (OWNER)", {._name = "Gandalf the Gray", ._owner = true}},
        {"Gandalf the Gray (ONITE)", {._name = "Gandalf the Gray", ._onite = true}},
        {"Gandalf the Gray (PRINT)", {._name = "Gandalf the Gray", ._print = true}},
        {"Gandalf the Gray (RECEIPT)", {._name = "Gandalf the Gray", ._receipt = true}},
        {"Gandalf the Gray (NO RECEIPT)", {._name = "Gandalf the Gray", ._no_receipt = true}},
        {"Gandalf the Gray (BOARD, INSTANT, LIST, OWNER, ONITE, PRINT, RECEIPT, NO RECEIPT)", {._name = "Gandalf the Gray", ._board = true, ._instant = true, ._list = true, ._owner = true, ._onite = true, ._print = true, ._receipt = true, ._no_receipt = true}},
        // clang-format on

    };
    for (const auto& t : cases) {
        SCOPED_TRACE(testing::Message() << "with test_data[" << t.first << "]");
        FirstLineExpectEqual(t.first, t.second);
    }
}

TEST(RawAddressSecond, invalid) {
    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        EXPECT_THROW(a.parse_field("ems:", ""), PduMalformedDataError);
    }

    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        EXPECT_THROW(a.parse_field("MBX:", "lama"), PduMalformedDataError);
    }

    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        a.parse_field("EMS:", "Some EMS");
        EXPECT_THROW(a.parse_field("MBX:", ""), PduMalformedDataError);
    }

    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        a.parse_field("EMS:", "Some EMS");
        EXPECT_THROW(a.parse_field("EMS:", "Another EMS"), PduMalformedDataError);
    }
}

TEST(RawAddressSecond, valid) {
    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        a.parse_field("EMS:", "INTERNET");
        a.parse_field("MBX:", "gandalf@hobbiton.org");

        EXPECT_EQ(a._name, "Gandalf the Gray");
        EXPECT_EQ(a._ems, "INTERNET");
        EXPECT_EQ(a._mbx[0], "gandalf@hobbiton.org");
    }

    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        a.parse_field("EMS:", "CompuServe");
        a.parse_field("MBX:", "CSI:GANDALF");

        EXPECT_EQ(a._name, "Gandalf the Gray");
        EXPECT_EQ(a._ems, "CompuServe");
        EXPECT_EQ(a._mbx[0], "CSI:GANDALF");
    }

    {
        RawAddress a;
        a.parse_first_line("Gandalf the Gray");
        a.parse_field("EMS:", "HOBBITONMAIL");
        a.parse_field("MBX:", "OR=Hobbiton");
        a.parse_field("MBX:", "UN=DT");
        a.parse_field("MBX:", "GI=Gandalf");

        EXPECT_EQ(a._name, "Gandalf the Gray");
        EXPECT_EQ(a._ems, "HOBBITONMAIL");
        EXPECT_EQ(a._mbx[0], "OR=Hobbiton");
        EXPECT_EQ(a._mbx[1], "UN=DT");
        EXPECT_EQ(a._mbx[2], "GI=Gandalf");
    }
}

#define DATETIME_GMT_VALID(string, date)                                                           \
    {                                                                                              \
        Date d;                                                                                    \
        d.parse(string);                                                                           \
        EXPECT_EQ(d.to_gmt_string(), date);                                                        \
    }

#define DATETIME_ZONE_VALID(zone)                                                                  \
    {                                                                                              \
        Date d;                                                                                    \
        d.parse("Sun Aug 11, 2024 07:03 PM " zone);                                                \
        EXPECT_EQ(d.to_orig_string(), "Sun Aug 11, 2024 07:03 PM " zone);                          \
    }

#define DATETIME_INVALID(string)                                                                   \
    {                                                                                              \
        Date d;                                                                                    \
        EXPECT_THROW(d.parse(string), std::invalid_argument);                                      \
    }

TEST(DateTest, invalid) {
    DATETIME_INVALID("");
    DATETIME_INVALID("WWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
    DATETIME_INVALID("Sun August 11, 2024 12:00 AM ");
    DATETIME_INVALID("Su  Aug 11, 2024 12:00 AM GMT");
    DATETIME_INVALID("Sun Mon 11, 2024 12:00 AM GMT");
    DATETIME_INVALID("Sun Aug 33, 2024 12:00 AM GMT");
    DATETIME_INVALID("Sun Aug 11, 2024 12:00 XD GMT");
    DATETIME_INVALID("Sun Aug 11, 2024 12:00 AM XXX");
}

TEST(DateTest, valid) {
    DATETIME_ZONE_VALID("AHS");
    DATETIME_ZONE_VALID("AHD");
    DATETIME_ZONE_VALID("YST");
    DATETIME_ZONE_VALID("YDT");
    DATETIME_ZONE_VALID("PST");
    DATETIME_ZONE_VALID("PDT");
    DATETIME_ZONE_VALID("MST");
    DATETIME_ZONE_VALID("MDT");
    DATETIME_ZONE_VALID("CST");
    DATETIME_ZONE_VALID("CDT");
    DATETIME_ZONE_VALID("EST");
    DATETIME_ZONE_VALID("EDT");
    DATETIME_ZONE_VALID("AST");
    DATETIME_ZONE_VALID("GMT");
    DATETIME_ZONE_VALID("BST");
    DATETIME_ZONE_VALID("WES");
    DATETIME_ZONE_VALID("WED");
    DATETIME_ZONE_VALID("EMT");
    DATETIME_ZONE_VALID("MTS");
    DATETIME_ZONE_VALID("MTD");
    DATETIME_ZONE_VALID("JST");
    DATETIME_ZONE_VALID("EAD");

    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM AHS", "Sun Aug 11, 2024 10:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM AHD", "Sun Aug 11, 2024 09:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM YST", "Sun Aug 11, 2024 09:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM PST", "Sun Aug 11, 2024 08:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM PDT", "Sun Aug 11, 2024 07:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM MST", "Sun Aug 11, 2024 07:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM MDT", "Sun Aug 11, 2024 06:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM CST", "Sun Aug 11, 2024 06:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM EDT", "Sun Aug 11, 2024 04:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM AST", "Sun Aug 11, 2024 04:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM GMT", "Sun Aug 11, 2024 12:00 AM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM BST", "Sat Aug 10, 2024 11:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM WES", "Sat Aug 10, 2024 11:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM WED", "Sat Aug 10, 2024 10:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM EMT", "Sat Aug 10, 2024 10:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM MTS", "Sat Aug 10, 2024 09:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM MTD", "Sat Aug 10, 2024 08:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM JST", "Sat Aug 10, 2024 03:00 PM GMT");
    DATETIME_GMT_VALID("Sun Aug 11, 2024 12:00 AM EAD", "Sat Aug 10, 2024 02:00 PM GMT");
}

class PduParserSyntaxErrorException : public ::testing::TestWithParam<std::string>,
                                      public PduParserTestBase {};
class PduParserChecksumErrorException : public ::testing::TestWithParam<std::string>,
                                        public PduParserTestBase {};
class PduParserMalformedDataErrorException : public ::testing::TestWithParam<std::string>,
                                             public PduParserTestBase {};

TEST_P(PduParserSyntaxErrorException, SyntaxError) {
    EXPECT_THROW(ParseLine(GetParam()), PduSyntaxError);
}

TEST_P(PduParserChecksumErrorException, ChecksumError) {
    EXPECT_THROW(ParseLine(GetParam()), PduChecksumError);
}

TEST_P(PduParserMalformedDataErrorException, MalformedData) {
    EXPECT_THROW(ParseLine(GetParam()), PduMalformedDataError);
}

INSTANTIATE_TEST_SUITE_P(Various, PduParserSyntaxErrorException,
                         // clang-format off
    testing::Values(
	"", 
	"/", 
	"NOT A SLASH\r", 
	"/     \r", 
	"/create\r",
	"/create*\r",
	"create*ZZZZ*\r",
	"/create*QWER\r",
	"/create invalid parameter*09B5\r",
	"/verify*zzzz\r",
	"/create/*ZZZZ\r",
	"//create*ZZZZ\r"
	));
// clang-format on

INSTANTIATE_TEST_SUITE_P(Various, PduParserChecksumErrorException,
                         testing::Values(
                             // clang-format off
						"/create*1234\r",
						"/verify\r\n/end verify*0000\r\n"
						));
// clang-format oon

TEST_F(PduParserTest, End) {
    ParseLine("/verify\r");
    EXPECT_THROW(ParseLine("/end verify garbage*ZZZ\r"), PduSyntaxError);
}

#define SIMPLE_PDU_TEST(line, expected_type)                                                       \
    TEST_F(PduParserTest, MACRO_CONCAT(__COUNTER__, expected_type)) {                              \
        ParseLine(line);                                                                           \
        EXPECT_TRUE(p.is_complete());                                                              \
        PduVariant pdu = p.extract_pdu();                                                          \
        EXPECT_TRUE(std::holds_alternative<expected_type>(pdu));                                   \
    }

SIMPLE_PDU_TEST("/create*ZZZZ\r\n", CreatePdu);
SIMPLE_PDU_TEST("/CREATE*020D\r\n", CreatePdu);
SIMPLE_PDU_TEST("/CrEaTe*026D\r\n", CreatePdu);
SIMPLE_PDU_TEST("/send *0223\r\n", SendPdu);
SIMPLE_PDU_TEST("/send\t*020C\r\n", SendPdu);
SIMPLE_PDU_TEST("/send \t *024C\r\n", SendPdu);
SIMPLE_PDU_TEST("/send*0203\r", SendPdu);
SIMPLE_PDU_TEST("/send *0223\r", SendPdu);
SIMPLE_PDU_TEST("/send\t*020C\r", SendPdu);
SIMPLE_PDU_TEST("/send \t *024C\r", SendPdu);
SIMPLE_PDU_TEST("/send*0203 \r", SendPdu);
SIMPLE_PDU_TEST("/send *0223\t\r", SendPdu);
SIMPLE_PDU_TEST("/send\t*020C \t \r", SendPdu);
SIMPLE_PDU_TEST("/send \t *024C\t\t\t\t\r", SendPdu);

SIMPLE_PDU_TEST("/busy*021C\r\n", BusyPdu);
SIMPLE_PDU_TEST("/create*02CD\r\n", CreatePdu);
SIMPLE_PDU_TEST("/term*0211\r\n", TermPdu);
SIMPLE_PDU_TEST("/send*0203\r\n", SendPdu);
SIMPLE_PDU_TEST("/scan*01FE\r\n", ScanPdu);
SIMPLE_PDU_TEST("/turn*0222\r\n", TurnPdu);

SIMPLE_PDU_TEST("/verify\r\nTo: Gandalf\r\n/end verify*0B01\r\n", VerifyPdu);
SIMPLE_PDU_TEST("/env\r\nTo: Gandalf\r\n/end env*0869\r\n", EnvPdu);
SIMPLE_PDU_TEST("/comment\r\nThis is a comment\r\n/end comment*0E1B\r\n", CommentPdu);

INSTANTIATE_TEST_SUITE_P(Scan, PduParserSyntaxErrorException,
                         // clang-format off
    testing::Values(
		"/scan FOLDER=((INBOX))*ZZZZ\r",
		"/scan FOLDER*ZZZZ\r",
		"/scan FOLDER=INBOX*ZZZZ\r",
		"/scan FOLDER=(INBOX), FOLDER=(OUTBOX)*ZZZZ\r",
		"/scan PRIORITY=something*ZZZZ\r"
	));
// clang-format on

INSTANTIATE_TEST_SUITE_P(Scan, PduParserMalformedDataErrorException,
                         // clang-format off
    testing::Values(
		"/scan FOLDER=(NOTREAL)*ZZZZ\r",
		"/scan SUBJECT=(Invalid%00Character)*ZZZZ\r"
	));
// clang-format on

// Scan and Turn PDUs have identical options
class ScanTest : public PduParserTest {
  protected:
    ScanPdu CreateScanPdu(const std::string& options) {
        ParseLine(std::format("/scan {} *ZZZZ\r\n", options));
        return std::get<ScanPdu>(p.extract_pdu());
    }

    TurnPdu CreateTurnPdu(const std::string& options) {
        ParseLine(std::format("/turn {} *ZZZZ\r\n", options));
        return std::get<TurnPdu>(p.extract_pdu());
    }

    void CompareScanField(const ScanPdu& pdu, auto args) {
        const auto& [field, value] = args;
        EXPECT_EQ((pdu.*field)(), value);
    }

    void CompareTurnField(const TurnPdu& pdu, auto args) {
        const auto& [field, value] = args;
        EXPECT_EQ((pdu.*field)(), value);
    }

    template <typename... Args> void CompareFields(const std::string& options, Args&&... args) {
        ScanPdu scanpdu = CreateScanPdu(options);
        TurnPdu turnpdu = CreateTurnPdu(options);
        (CompareScanField(scanpdu, args), ...);
        (CompareTurnField(turnpdu, args), ...);
    }
};

TEST_F(ScanTest, valid) {
    // clang-format off
    CompareFields("", 
    	std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::inbox));
    CompareFields("FOLDER=(INBOX)",
    	std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::inbox));
    CompareFields("FOLDER=(INBOX) ",
    	std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::inbox));
    CompareFields("FOLDER=(INBOX) \t\t\t",
		std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::inbox));
    CompareFields("FOLDER=(OUTBOX)",
		std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::outbox));
    CompareFields("FOLDER=(DESK)",
    	std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::desk));
    CompareFields("FOLDER=(TRASH)",
    	std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::trash));
    CompareFields("FOLDER=(OUTBOX),FOLDER=(TRASH)",
		std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::trash));
	CompareFields("FOLDER=(OUTBOX),SUBJECT=(Subject Line)",
		std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::outbox),
		std::pair(&ScanPdu::get_subject, "Subject Line"));
	CompareFields("FOLDER=(OUTBOX),FROM=(Gandalf The Gray)",
		std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::outbox),
		std::pair(&ScanPdu::get_from, "Gandalf The Gray"));
	CompareFields("FOLDER=(OUTBOX),FROM=(Gandalf The Gray),SUBJECT=(Subject Line)",
		std::pair(&ScanPdu::get_folder_id, ScanPdu::folder_id::outbox),
		std::pair(&ScanPdu::get_from, "Gandalf The Gray"),
		std::pair(&ScanPdu::get_subject, "Subject Line"));
    // clang-format on
}

INSTANTIATE_TEST_SUITE_P(Verify, PduParserSyntaxErrorException,
                         // clang-format off
    testing::Values(
		"/verify*ZZZZ\r",
		"/verify\r\n/end verify*ZZZZ",
		"/verify\r\n/end verify*ZZZ\r\n",
		"/verify\r\n/end verify*",
		"/verify\r\n/end text*ZZZZ\r\n"
	));
// clang-format on

INSTANTIATE_TEST_SUITE_P(
    Verify, PduParserMalformedDataErrorException,
    // clang-format off
    testing::Values(
		"/verify NONEEXISTANT\r",
		"/verify STUFF STUFF\r",
		// Unescaped "/" in address
		"/verify\r\nTo: Gandalf/111-1111\r\n/end verify*ZZZZ\r\n",
		// Invalid options
		"/verify\r\nTo: Gandalf (,)\r\n/end verify*ZZZZ\r\n",
		"/verify\r\nTo: Gandalf (,BOARD)\r\n/end verify*ZZZZ\r\n",
		"/verify\r\nTo: Gandalf (NONEXISTANT)\r\n/end verify*ZZZZ\r\n"
	));
// clang-format on

TEST_F(PduParserTest, Comment) {
    EXPECT_THROW(ParseLine("/comment\r\nInvalid / in text\r\n/end comment*zzzz\r\n"),
                 PduMalformedDataError);
}

TEST_F(PduParserTest, Verify) {
    EXPECT_THROW(ParseLine("/verify\r\n/end verify*zzzz\r\n"), PduNoEnvelopeDataError);
    EXPECT_THROW(ParseLine("/verify\r\nCc: Gandalf\r\n/end verify*zzzz\r\n"), PduToRequiredError);
}

template <class P> class VerifyEnvTest : public PduParserTest {
  protected:
    P CreatePdu(const std::string& line) {
        ParseLine(line);
        return std::get<P>(p.extract_pdu());
    }

    void CompareField(const P& pdu, auto args) {
        const auto& [field, value] = args;
        EXPECT_EQ((pdu.*field)(), value);
    }

    template <typename... Args>
    void CompareFields(const std::string& line, const std::vector<RawAddress>& raw_to,
                       const std::vector<RawAddress>& raw_cc, Args&&... args) {
        P pdu = CreatePdu(line);
        EXPECT_EQ(pdu.get_to_address().size(), raw_to.size());
        EXPECT_EQ(pdu.get_cc_address().size(), raw_cc.size());
        for (size_t i = 0; i < pdu.get_to_address().size(); ++i) {
            EXPECT_EQ(pdu.get_to_address()[i], raw_to[i]);
        }
        for (size_t i = 0; i < pdu.get_cc_address().size(); ++i) {
            EXPECT_EQ(pdu.get_cc_address()[i], raw_cc[i]);
        }
        (CompareField(pdu, args), ...);
    }
};

typedef VerifyEnvTest<VerifyPdu> VerifyTest;
typedef VerifyEnvTest<EnvPdu> EnvTest;

TEST_F(VerifyTest, priority) {
#define GANDALF "To: Gandalf %2F 111-1111\r\n"
#define END "/end verify*zzzz\r\n"
    std::vector<RawAddress> gandalf_to = {{._name = "Gandalf", ._id = "111-1111"}};

    CompareFields("/verify\r\n" GANDALF END, gandalf_to, {},
                  std::pair{&VerifyPdu::get_priority_id, VerifyPdu::priority_id::none});
    CompareFields("/verify POSTAL\r\n" GANDALF END, gandalf_to, {},
                  std::pair{&VerifyPdu::get_priority_id, VerifyPdu::priority_id::postal});
    CompareFields("/verify ONITE\r\n" GANDALF END, gandalf_to, {},
                  std::pair{&VerifyPdu::get_priority_id, VerifyPdu::priority_id::onite});
#undef GANDALF
#undef END
}

TEST_F(VerifyTest, addresses) {
#define GANDALF "To: Gandalf"
#define FRODO "CC: Frodo"
#define END "/end verify*zzzz\r\n"

    CompareFields("/verify\r\n" GANDALF "\r\n" END, {{._name = "Gandalf"}}, {});
    CompareFields("/verify\r\n" GANDALF "(BOARD)\r\n" END, {{._name = "Gandalf", ._board = true}},
                  {});
    CompareFields("/verify\r\n" GANDALF "\r\n" FRODO "\r\n" END, {{._name = "Gandalf"}},
                  {{._name = "Frodo"}});
#undef GANDALF
#undef FRODO
#undef END
}

TEST_F(EnvTest, priority) {
#define GANDALF "To: Gandalf %2F 111-1111\r\n"
#define END "/end env*zzzz\r\n"
    std::vector<RawAddress> gandalf_to = {{._name = "Gandalf", ._id = "111-1111"}};

    CompareFields("/env\r\n" GANDALF END, gandalf_to, {},
                  std::pair{&VerifyPdu::get_priority_id, VerifyPdu::priority_id::none});
    CompareFields("/env POSTAL\r\n" GANDALF END, gandalf_to, {},
                  std::pair{&VerifyPdu::get_priority_id, VerifyPdu::priority_id::postal});
    CompareFields("/env ONITE\r\n" GANDALF END, gandalf_to, {},
                  std::pair{&VerifyPdu::get_priority_id, VerifyPdu::priority_id::onite});
#undef GANDALF
#undef END
}

TEST_F(EnvTest, addresses) {
#define GANDALF "To: Gandalf"
#define FRODO "CC: Frodo"
#define END "/end env*zzzz\r\n"

    CompareFields("/env\r\n" GANDALF "\r\n" END, {{._name = "Gandalf"}}, {});
    CompareFields("/env\r\n" GANDALF "(BOARD)\r\n" END, {{._name = "Gandalf", ._board = true}}, {});
    CompareFields("/env\r\n" GANDALF "\r\n" FRODO "\r\n" END, {{._name = "Gandalf"}},
                  {{._name = "Frodo"}});
#undef GANDALF
#undef FRODO
#undef END
}

TEST_F(EnvTest, fields) {
#define START "/env\r\nTo: Gandalf\r\n"
#define END "/end env*zzzz\r\n"
#define SUBJECT "A very fine subject"
#define MESSAGEID "A very fine message ID"
    std::vector<RawAddress> gandalf_to = {{._name = "Gandalf"}};
    std::vector<std::string> source_messageid = {
        "source Special-message id 2", "source Special-message id 3", "source Special-message id 4",
        "source Special-message id 5", "source Special-message id 6"};
    std::vector<std::pair<std::string, std::string>> u_headers = {
        {"U-BLAH1", "Unknown custom field 2"},
        {"U-GODOT", "Unknown custom field 3"},
        {"U-LLAMAS-ONE-TWO", "Unknown custom field 4"},
        {"U-AND_OTHER-CHARS", "Unknown custom field 5"},
        {"u-the-last-one", "Unknown custom field 6"}

    };
    Date d;
    d.parse("Sun Aug 11, 2024 12:00 AM GMT");

    CompareFields(START END, gandalf_to, {}, std::pair(&EnvPdu::has_date, false),
                  std::pair(&EnvPdu::has_source_date, false));

    CompareFields(START "Date: Sun Aug 11, 2024 12:00 AM GMT\r\n" END, gandalf_to, {},
                  std::pair(&EnvPdu::get_date, d), std::pair(&EnvPdu::has_source_date, false));
    CompareFields(START "Source-Date: Sun Aug 11, 2024 12:00 AM GMT\r\n" END, gandalf_to, {},
                  std::pair(&EnvPdu::get_source_date, d), std::pair(&EnvPdu::has_date, false));

    CompareFields(START "Subject:" SUBJECT "\r\n" END, gandalf_to, {},
                  std::pair(&EnvPdu::get_subject, SUBJECT));

    CompareFields(START "Message-id:" MESSAGEID "\r\n" END, gandalf_to, {},
                  std::pair(&EnvPdu::get_message_id, MESSAGEID));

    // We only store the last 5 source-message ids
    CompareFields(START "source-Message-ID: source Special-message id 1\r\n"
                        "source-Message-ID: source Special-message id 2\r\n"
                        "source-Message-ID: source Special-message id 3\r\n"
                        "source-Message-ID: source Special-message id 4\r\n"
                        "source-Message-ID: source Special-message id 5\r\n"
                        "source-Message-ID: source Special-message id 6\r\n" END,
                  gandalf_to, {}, std::pair(&EnvPdu::get_source_message_id, source_messageid));

    // We only store the last 5 custom u- headers
    CompareFields(START "U-SOMETHING1: Unknown custom field 1\r\n"
                        "U-BLAH1: Unknown custom field 2\r\n"
                        "U-GODOT: Unknown custom field 3\r\n"
                        "U-LLAMAS-ONE-TWO: Unknown custom field 4\r\n"
                        "U-AND_OTHER-CHARS: Unknown custom field 5\r\n"
                        "u-the-last-one: Unknown custom field 6\r\n" END,
                  gandalf_to, {}, std::pair(&EnvPdu::get_u_fields, u_headers));

    CompareFields(START "From: Frodo\r\n" END, gandalf_to, {},
                  std::pair(&EnvPdu::get_from_address, RawAddress{._name = "Frodo"}));
#undef START
#undef END
#undef SUBJECT
#undef MESSAGEID
}

TEST_F(PduParserTest, invalidEnv) {
    EXPECT_THROW(ParseLine("/env\rTo: Bilbo\rFrom:Gandalf\rFrom:Frodo\r/end env*zzzz\r"),
                 PduEnvelopeDataError);
}

class TextFixture : public PduParserTest {
  protected:
    void ExpectCorrectType(const std::string& type_text, TextPdu::content_type expected) {
        ParseLine(std::format("/text {}\r\n/end text*zzzz\r\n", type_text));
        TextPdu pdu = std::get<TextPdu>(p.extract_pdu());
        EXPECT_EQ(pdu.get_content_type(), expected);
    }

    void ExpectDescription(const std::string& description, const std::string& expected) {
        std::string line = std::format("/text ASCII:{}\r\n/end text*zzzz\r\n", description);
        ParseLine(line);
        TextPdu pdu = std::get<TextPdu>(p.extract_pdu());
        EXPECT_TRUE(pdu.has_description());
        EXPECT_EQ(pdu.get_description(), expected);
    }
};

TEST_F(TextFixture, ContentTypes) {
    const std::vector<std::pair<const std::string, TextPdu::content_type>> types = {
        {"", TextPdu::content_type::ascii},           {"ASCII", TextPdu::content_type::ascii},
        {"PRINTABLE", TextPdu::content_type::ascii},  {"ENV", TextPdu::content_type::env},
        {"BINARY", TextPdu::content_type::binary},    {"G3FAX", TextPdu::content_type::binary},
        {"TLX", TextPdu::content_type::binary},       {"VOICE", TextPdu::content_type::binary},
        {"TIF0", TextPdu::content_type::binary},      {"TIF1", TextPdu::content_type::binary},
        {"TTX", TextPdu::content_type::binary},       {"VIDEOTEX", TextPdu::content_type::binary},
        {"ENCRYPTED", TextPdu::content_type::binary}, {"SFD", TextPdu::content_type::binary},
        {"RACAL", TextPdu::content_type::binary},
    };

    for (const auto& t : types) {
        SCOPED_TRACE(testing::Message() << "with test_data[" << t.first << "]");
        ExpectCorrectType(t.first, t.second);
    }
}

TEST_F(TextFixture, Description) {
    const std::vector<std::pair<const std::string, const std::string>> descriptions = {
        {"description", "description"},
        {" description ", "description"},
        {"\tdescription\t", "description"},
        {"text%2Fplain", "text/plain"},
        {"sfj4dc.BOB", "sfj4dc.BOB"},
        {" description with spaces", "description with spaces"}};

    for (const auto& t : descriptions) {
        SCOPED_TRACE(testing::Message()
                     << "with test_data['" << t.first << "', '" << t.second << "']");
        ExpectDescription(t.first, t.second);
    }
}

class TemporaryStorageTest : public AsyncTest {
  protected:
    std::filesystem::path temp_root;

    void SetUp() override {
        temp_root = "runtime_tmp";
        std::filesystem::remove_all(temp_root);
        std::filesystem::create_directories(temp_root);
    }

    void TearDown() override { std::filesystem::remove_all(temp_root); }
};

TEST_F(TemporaryStorageTest, valid) {
    RunAsync([&]() -> asio::awaitable<void> {
        const std::string data = "This is some file data\r\n";
        std::filesystem::path temp_path = temp_root / "data";
        temp_path = temp_path / "lama";

        TemporaryStorage p(io_context, temp_path, 1024);
        TemporaryFile f = p.create_file();
        size_t bytes = co_await f.write(data);
        EXPECT_EQ(bytes, data.size());
        EXPECT_TRUE(f.close());

        std::filesystem::path file_path = temp_path / f.get_filename();
        std::ifstream file(file_path);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        EXPECT_EQ(content, data);
    });
}
