#include <print>
#include <string_view>
#include <variant>

#include <gtest/gtest.h>

#include "address.hpp"
#include "date.hpp"
#include "mep2_errors.hpp"
#include "mep2_pdu.hpp"
#include "mep2_pdu_parser.hpp"
#include "string_utils.hpp"

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
        {"Strip top bits: \xc1\xd3\xc3\xc9\xc9", "Strip top bits: ASCII"}};

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

TEST(is_mciid, valid) {
    EXPECT_EQ(is_mciid("111-1111"), true);
    EXPECT_EQ(is_mciid("111-111-1111"), true);
    EXPECT_EQ(is_mciid("000-111-1111"), true);
    EXPECT_EQ(is_mciid("0001111111"), true);
    EXPECT_EQ(is_mciid("1111111"), true);
    EXPECT_EQ(is_mciid("1111111111"), true);
    EXPECT_EQ(is_mciid("111-1111111"), false);
    EXPECT_EQ(is_mciid("111--1111111"), false);
    EXPECT_EQ(is_mciid("1111-111"), false);
    EXPECT_EQ(is_mciid("NOT-REAL"), false);
    EXPECT_EQ(is_mciid("NOT-VAL-IDSE"), false);
}

class RawAddressFixture : public ::testing::Test {
  protected:
    void FirstLineExpectEqual(const std::string& line, const RawAddress& expected) {
        RawAddress a;
        a.parse_first_line(line);
        EXPECT_EQ(a, expected);
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

TEST(ParserTest, invalid) {
    PduParser p;
    EXPECT_THROW(p.parse_line(""), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/"), PduSyntaxError);

    EXPECT_THROW(p.parse_line("NOT A SLASH\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/create\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/create*\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/create*"), PduSyntaxError);

    EXPECT_THROW(p.parse_line("/create*ZZZZ*\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/create*QWER\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/create invalid parameter*09B5\r"), PduSyntaxError);

    EXPECT_THROW(p.parse_line("/create*1234\r"), PduChecksumError);
    EXPECT_THROW(p.parse_line("/verify*zzzz\r"), PduSyntaxError);

    EXPECT_THROW(p.parse_line("/create/*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("//create*ZZZZ\r"), PduSyntaxError);

    p.parse_line("/verify\r\n");
    EXPECT_THROW(p.parse_line("/end verify garbage*ZZZ\r\n"), PduSyntaxError);
}

#define TEST_SINGLE_PDU(input, expected_type)                                                      \
    {                                                                                              \
        p.parse_line(input);                                                                       \
        EXPECT_TRUE(p.is_complete());                                                              \
        PduVariant pdu = p.extract_pdu();                                                          \
        EXPECT_TRUE(std::holds_alternative<expected_type>(pdu));                                   \
    }

#define TEST_MULTI_PDU(input, expected_type)                                                       \
    {                                                                                              \
        std::string_view sv(input);                                                                \
        while (!sv.empty()) {                                                                      \
            size_t l = sv.find_first_of("\r\n");                                                   \
            if (l == std::string_view::npos)                                                       \
                break;                                                                             \
            p.parse_line(sv.substr(0, l + 2));                                                     \
            sv.remove_prefix(l + 2);                                                               \
        }                                                                                          \
        EXPECT_TRUE(p.is_complete());                                                              \
        PduVariant pdu = p.extract_pdu();                                                          \
        EXPECT_TRUE(std::holds_alternative<expected_type>(pdu));                                   \
    }

TEST(ParserTest, valid) {
    PduParser p;
    TEST_SINGLE_PDU("/create*ZZZZ\r\n", CreatePdu);
    TEST_SINGLE_PDU("/CREATE*020D\r\n", CreatePdu);
    TEST_SINGLE_PDU("/CrEaTe*026D\r\n", CreatePdu);
    TEST_SINGLE_PDU("/send *0223\r\n", SendPdu);
    TEST_SINGLE_PDU("/send\t*020C\r\n", SendPdu);
    TEST_SINGLE_PDU("/send \t *024C\r\n", SendPdu);
    TEST_SINGLE_PDU("/send*0203\r", SendPdu);
    TEST_SINGLE_PDU("/send *0223\r", SendPdu);
    TEST_SINGLE_PDU("/send\t*020C\r", SendPdu);
    TEST_SINGLE_PDU("/send \t *024C\r", SendPdu);
    TEST_SINGLE_PDU("/send*0203 \r", SendPdu);
    TEST_SINGLE_PDU("/send *0223\t\r", SendPdu);
    TEST_SINGLE_PDU("/send\t*020C \t \r", SendPdu);
    TEST_SINGLE_PDU("/send \t *024C\t\t\t\t\r", SendPdu);

    TEST_SINGLE_PDU("/busy*021C\r\n", BusyPdu);
    TEST_SINGLE_PDU("/create*02CD\r\n", CreatePdu);
    TEST_SINGLE_PDU("/term*0211\r\n", TermPdu);
    TEST_SINGLE_PDU("/send*0203\r\n", SendPdu);
    TEST_SINGLE_PDU("/scan*01FE\r\n", ScanPdu);
    TEST_SINGLE_PDU("/turn*0222\r\n", TurnPdu);

    TEST_MULTI_PDU("/verify\r\nTo: Gandalf\r\n/end verify*0B01\r\n", VerifyPdu);
    TEST_MULTI_PDU("/env\r\nTo: Gandalf\r\n/end env*0869\r\n", EnvPdu);

    TEST_MULTI_PDU("/comment\r\nThis is a comment\r\n/end comment*0E1B\r\n", CommentPdu);
}

TEST(ScanTest, invalid) {
    PduParser p;
    EXPECT_THROW(p.parse_line("/scan FOLDER=((INBOX))*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/scan FOLDER*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/scan FOLDER=INBOX*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/scan FOLDER=(INBOX), FOLDER=(OUTBOX)*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/scan PRIORITY=something*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/scan FOLDER=(NOTREAL)*ZZZZ\r"), PduMalformedDataError);

    EXPECT_THROW(p.parse_line("/scan SUBJECT=(Invalid%00Character)*ZZZZ\r"), PduMalformedDataError);
}

#define SCANPDU_CREATE(option)                                                                     \
    PduParser p;                                                                                   \
    p.parse_line("/scan " option "*ZZZZ\r\n");                                                     \
    ScanPdu pdu = std::get<ScanPdu>(p.extract_pdu());

#define SCANPDU_VALID(option, field, value)                                                        \
    {                                                                                              \
        SCANPDU_CREATE(option);                                                                    \
        EXPECT_EQ(pdu.field(), value);                                                             \
    }

TEST(ScanTest, valid) {
    PduParser p;

    SCANPDU_VALID("", get_folder_id, ScanPdu::folder_id::inbox);
    SCANPDU_VALID("FOLDER=(INBOX)", get_folder_id, ScanPdu::folder_id::inbox);
    SCANPDU_VALID("FOLDER=(INBOX) ", get_folder_id, ScanPdu::folder_id::inbox);
    SCANPDU_VALID("FOLDER=(INBOX) \t\t\t ", get_folder_id, ScanPdu::folder_id::inbox);

    SCANPDU_VALID("FOLDER=(OUTBOX)", get_folder_id, ScanPdu::folder_id::outbox);
    SCANPDU_VALID("FOLDER=(DESK)", get_folder_id, ScanPdu::folder_id::desk);
    SCANPDU_VALID("FOLDER=(TRASH)", get_folder_id, ScanPdu::folder_id::trash);
    SCANPDU_VALID("FOLDER=(OUTBOX),FOLDER=(TRASH)", get_folder_id, ScanPdu::folder_id::trash);

    {
        SCANPDU_CREATE("FOLDER=(OUTBOX),SUBJECT=(Some kind of subject)");
        EXPECT_EQ(pdu.get_folder_id(), ScanPdu::folder_id::outbox);
        EXPECT_EQ(pdu.get_subject(), "Some kind of subject");
    }

    {
        SCANPDU_CREATE("FROM=(Gandalf the Gray)");
        EXPECT_EQ(pdu.get_folder_id(), ScanPdu::folder_id::inbox);
        EXPECT_EQ(pdu.get_from(), "Gandalf the Gray");
    }

    {
        SCANPDU_CREATE("FOLDER=(OUTBOX),SUBJECT=(Some kind of "
                       "subject),FROM=(Gandalf the Gray)");
        EXPECT_EQ(pdu.get_folder_id(), ScanPdu::folder_id::outbox);
        EXPECT_EQ(pdu.get_subject(), "Some kind of subject");
        EXPECT_EQ(pdu.get_from(), "Gandalf the Gray");
    }
}

// Scan and Turn PDUs use the same code
TEST(TurnTest, valid) {
    PduParser p;
    {
        p.parse_line("/turn FROM=(Gandalf the Gray)*ZZZZ\r");
        TurnPdu pdu = std::get<TurnPdu>(p.extract_pdu());
        EXPECT_EQ(pdu.get_folder_id(), TurnPdu::folder_id::inbox);
        EXPECT_EQ(pdu.get_from(), "Gandalf the Gray");
    }
}

#define VERIFYPDU_INVALID(line, exception)                                                         \
    {                                                                                              \
        PduParser p;                                                                               \
        p.parse_line("/verify\r\n");                                                               \
        EXPECT_THROW(p.parse_line(line), exception);                                               \
    }

TEST(CommentTest, invalid) {
    {
        PduParser p;
        p.parse_line("/comment\r\n");
        p.parse_line("Invalid / in text\r\n");
        EXPECT_THROW(p.parse_line("/end comment*zzzz\r\n"), PduMalformedDataError);
    }
}

TEST(VerifyTest, invalid) {
    PduParser p;
    EXPECT_THROW(p.parse_line("/verify*ZZZZ\r"), PduSyntaxError);
    EXPECT_THROW(p.parse_line("/verify NONEEXISTANT\r"), PduMalformedDataError);
    EXPECT_THROW(p.parse_line("/verify STUFF STUFF\r"), PduMalformedDataError);

    VERIFYPDU_INVALID("/end verify*0000\r\n", PduChecksumError);
    VERIFYPDU_INVALID("/end verify*ZZZZ", PduSyntaxError);
    VERIFYPDU_INVALID("/end verify*ZZZ\r\n", PduSyntaxError);
    VERIFYPDU_INVALID("/end verify*", PduSyntaxError);
    VERIFYPDU_INVALID("/end text*ZZZZ\r\n", PduSyntaxError);
    VERIFYPDU_INVALID("/end verify*ZZZZ\r\n", PduNoEnvelopeDataError);

    {
        PduParser p;
        p.parse_line("/verify\r\n");
        p.parse_line("Cc: Gandalf\r\n");
        EXPECT_THROW(p.parse_line("/end verify*ZZZZ\r"), PduToRequiredError);
    }

    // Unescaped '/' in address
    {
        PduParser p;
        p.parse_line("/verify\r\n");
        p.parse_line("To: Gandalf/111-1111\r\n");
        EXPECT_THROW(p.parse_line("/end verify*ZZZZ\r\n"), PduMalformedDataError);
    }

    // Invalid options
    {
        PduParser p;
        p.parse_line("/verify\r\n");
        p.parse_line("To: Gandalf (,)\r\n");
        EXPECT_THROW(p.parse_line("/end verify*ZZZZ\r\n"), PduMalformedDataError);
    }

    {
        PduParser p;
        p.parse_line("/verify\r\n");
        p.parse_line("To: Gandalf (,BOARD)\r\n");
        EXPECT_THROW(p.parse_line("/end verify*ZZZZ\r\n"), PduMalformedDataError);
    }

    {
        PduParser p;
        p.parse_line("/verify\r\n");
        p.parse_line("To: Gandalf (NONEXISTANT)\r\n");
        EXPECT_THROW(p.parse_line("/end verify*ZZZZ\r\n"), PduMalformedDataError);
    }
}

#define VERIFYPDU_VALID(option, field, value)                                                      \
    {                                                                                              \
        PduParser p;                                                                               \
        p.parse_line("/verify " option " \r\n");                                                   \
        p.parse_line("To: Gandalf\r\n");                                                           \
        p.parse_line("/end verify*ZZZZ\r\n");                                                      \
        VerifyPdu pdu = std::get<VerifyPdu>(p.extract_pdu());                                      \
        EXPECT_EQ(pdu.field(), value);                                                             \
    }

#define VERIFYPDU_ADDRESS_VALID(addresses, raw_to, raw_cc)                                         \
    {                                                                                              \
        PduParser p;                                                                               \
        p.parse_line("/verify\r\n");                                                               \
        std::string_view sv(addresses);                                                            \
        while (!sv.empty()) {                                                                      \
            size_t l = sv.find_first_of("\r\n");                                                   \
            if (l == std::string_view::npos)                                                       \
                break;                                                                             \
            p.parse_line(sv.substr(0, l + 2));                                                     \
            sv.remove_prefix(l + 2);                                                               \
        }                                                                                          \
        p.parse_line("/end verify*ZZZZ\r\n");                                                      \
        VerifyPdu pdu = std::get<VerifyPdu>(p.extract_pdu());                                      \
        EXPECT_EQ(pdu.get_to_address().size(), raw_to.size());                                     \
        EXPECT_EQ(pdu.get_cc_address().size(), raw_cc.size());                                     \
        for (size_t i = 0; i < pdu.get_to_address().size(); ++i) {                                 \
            EXPECT_EQ(pdu.get_to_address()[i], raw_to[i]);                                         \
        }                                                                                          \
        for (size_t i = 0; i < pdu.get_cc_address().size(); ++i) {                                 \
            EXPECT_EQ(pdu.get_cc_address()[i], raw_cc[i]);                                         \
        }                                                                                          \
    }

TEST(VerifyTest, valid) {
    PduParser p;
    VERIFYPDU_VALID("", get_priority_id, VerifyPdu::priority_id::none);
    VERIFYPDU_VALID("POSTAL", get_priority_id, VerifyPdu::priority_id::postal);
    VERIFYPDU_VALID("ONITE", get_priority_id, VerifyPdu::priority_id::onite);

    std::vector<RawAddress> empty;
    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf\r\n", raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf ()\r\n", raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf", ._board = true, ._instant = true});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf (BOARD,INSTANT)\r\n", raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back(
            {._name = "Gandalf", ._board = true, ._instant = true, ._list = true, ._owner = true});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf (BOARD, INSTANT, LIST, OWNER)\r\n", raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf", ._id = "111-1111"});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf%2F111-1111\r\n", raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf", ._id = "111-1111"});
        std::vector<RawAddress> raw_cc;
        raw_cc.push_back({._name = "Frodo", ._id = "222-2222"});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf%2F111-1111\r\nCc: Frodo %2f 222-2222\r\n", raw_to,
                                raw_cc);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back(
            {._name = "Gandalf", ._ems = "Internet", ._mbx = {"gandalf@hobbiton.org"}});
        VERIFYPDU_ADDRESS_VALID("To: Gandalf\r\n Ems: Internet\r\n Mbx: gandalf@hobbiton.org\r\n",
                                raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({
            ._name = "Gandalf",
            ._organization = "The Good Guys",
            ._location = "Hobbiton",
        });
        VERIFYPDU_ADDRESS_VALID("To: Gandalf %2F Loc: Hobbiton %2F Org: The Good Guys \r\n", raw_to,
                                empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({
            ._name = "Gandalf",
            ._organization = "The Good Guys",
            ._location = "Hobbiton",
        });
        VERIFYPDU_ADDRESS_VALID("To: Gandalf%2FLoc:Hobbiton%2FOrg:The Good Guys\r\n", raw_to,
                                empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({
            ._name = "Gandalf",
            ._unresolved_org_loc_1 = "Hobbiton",
            ._unresolved_org_loc_2 = "The Good Guys",
        });
        VERIFYPDU_ADDRESS_VALID("To: Gandalf %2F Hobbiton %2F The Good Guys \r\n", raw_to, empty);
    }

    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back(
            {._name = "Gandalf", ._ems = "Internet", ._mbx = {"gandalf@hobbiton.org"}});
        raw_to.push_back({._name = "Frodo", ._ems = "Internet", ._mbx = {"frodo@hobbiton.org"}});

        VERIFYPDU_ADDRESS_VALID("To: Gandalf\r\n"
                                " Ems: Internet\r\n"
                                " Mbx: gandalf@hobbiton.org\r\n"
                                "To: Frodo\r\n"
                                "\tEms: Internet\r\n"
                                "\tMbx: frodo@hobbiton.org\r\n",
                                raw_to, empty);
    }
}

#define ENVPDU_ADDRESS_VALID(addresses, raw_to, raw_cc, from)                                      \
    PduParser p;                                                                                   \
    p.parse_line("/env\r\n");                                                                      \
    std::string_view sv(addresses);                                                                \
    while (!sv.empty()) {                                                                          \
        size_t l = sv.find_first_of("\r\n");                                                       \
        if (l == std::string_view::npos)                                                           \
            break;                                                                                 \
        p.parse_line(sv.substr(0, l + 2));                                                         \
        sv.remove_prefix(l + 2);                                                                   \
    }                                                                                              \
    p.parse_line("/end env*ZZZZ\r\n");                                                             \
    EnvPdu pdu = std::get<EnvPdu>(p.extract_pdu());                                                \
    EXPECT_EQ(pdu.get_to_address().size(), raw_to.size());                                         \
    EXPECT_EQ(pdu.get_cc_address().size(), raw_cc.size());                                         \
    for (size_t i = 0; i < pdu.get_to_address().size(); ++i) {                                     \
        EXPECT_EQ(pdu.get_to_address()[i], raw_to[i]);                                             \
    }                                                                                              \
    for (size_t i = 0; i < pdu.get_cc_address().size(); ++i) {                                     \
        EXPECT_EQ(pdu.get_cc_address()[i], raw_cc[i]);                                             \
    }                                                                                              \
    EXPECT_TRUE(pdu.has_from_address());                                                           \
    EXPECT_EQ(pdu.get_from_address(), from);

TEST(EnvTest, invalid) {
    {
        PduParser p;
        p.parse_line("/env\r\n");
        p.parse_line("To: Bilbo\r\n");
        p.parse_line("From: Gandalf\r\n");
        p.parse_line("From: Frodo\r\n");
        EXPECT_THROW(p.parse_line("/end env*ZZZZ\r"), PduEnvelopeDataError);
    }
}

TEST(EnvTest, valid) {
    PduParser p;
    std::vector<RawAddress> empty;
    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\nFrom: Frodo\r\n", raw_to, empty, from);
        EXPECT_FALSE(pdu.has_date());
        EXPECT_FALSE(pdu.has_source_date());
    }
    {
        Date d;
        d.parse("Sun Aug 11, 2024 12:00 AM GMT");

        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\n"
                             "From: Frodo\r\n"
                             "Date: Sun Aug 11, 2024 12:00 AM GMT\r\n",
                             raw_to, empty, from);
        EXPECT_TRUE(pdu.has_date());
        EXPECT_EQ(pdu.get_date(), d);
        EXPECT_FALSE(pdu.has_source_date());
    }
    {
        Date d;
        d.parse("Sun Aug 11, 2024 12:00 AM GMT");
        Date d2;
        d2.parse("Fri Aug 11, 2023 12:00 AM GMT");

        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\n"
                             "From: Frodo\r\n"
                             "Date: Sun Aug 11, 2024 12:00 AM GMT\r\n"
                             "Source-Date: Fri Aug 11, 2023 12:00 AM GMT\r\n",
                             raw_to, empty, from);
        EXPECT_TRUE(pdu.has_date());
        EXPECT_EQ(pdu.get_date(), d);
        EXPECT_TRUE(pdu.has_source_date());
        EXPECT_EQ(pdu.get_source_date(), d2);
    }
    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\n"
                             "From: Frodo\r\n"
                             "Subject: I hate this ring\r\n",
                             raw_to, empty, from);
        EXPECT_TRUE(pdu.has_subject());
        EXPECT_EQ(pdu.get_subject(), "I hate this ring");
    }
    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\n"
                             "From: Frodo\r\n"
                             "Message-ID: Special-message id\r\n",
                             raw_to, empty, from);
        EXPECT_TRUE(pdu.has_message_id());
        EXPECT_EQ(pdu.get_message_id(), "Special-message id");
    }
    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\n"
                             "From: Frodo\r\n"
                             "source-Message-ID: source Special-message id 1\r\n"
                             "source-Message-ID: source Special-message id 2\r\n"
                             "source-Message-ID: source Special-message id 3\r\n"
                             "source-Message-ID: source Special-message id 4\r\n"
                             "source-Message-ID: source Special-message id 5\r\n"
                             "source-Message-ID: source Special-message id 6\r\n",

                             raw_to, empty, from);
        EXPECT_TRUE(pdu.has_source_message_id());
        EXPECT_EQ(pdu.get_source_message_id()[0], "source Special-message id 2");
        EXPECT_EQ(pdu.get_source_message_id()[1], "source Special-message id 3");
        EXPECT_EQ(pdu.get_source_message_id()[2], "source Special-message id 4");
        EXPECT_EQ(pdu.get_source_message_id()[3], "source Special-message id 5");
        EXPECT_EQ(pdu.get_source_message_id()[4], "source Special-message id 6");
    }
    {
        std::vector<RawAddress> raw_to;
        raw_to.push_back({._name = "Gandalf"});
        RawAddress from = {._name = "Frodo"};

        ENVPDU_ADDRESS_VALID("To: Gandalf\r\n"
                             "From: Frodo\r\n"
                             "U-SOMETHING1: Unknown custom field 1\r\n"
                             "U-BLAH1: Unknown custom field 2\r\n"
                             "U-GODOT: Unknown custom field 3\r\n"
                             "U-LLAMAS-ONE-TWO: Unknown custom field 4\r\n"
                             "U-AND_OTHER-CHARS: Unknown custom field 5\r\n"
                             "u-the-last-one: Unknown custom field 6\r\n",

                             raw_to, empty, from);
        EXPECT_TRUE(pdu.has_u_fields());
        EXPECT_EQ(pdu.get_u_fields()[0].first, "U-BLAH1");
        EXPECT_EQ(pdu.get_u_fields()[0].second, "Unknown custom field 2");

        EXPECT_EQ(pdu.get_u_fields()[1].first, "U-GODOT");
        EXPECT_EQ(pdu.get_u_fields()[1].second, "Unknown custom field 3");

        EXPECT_EQ(pdu.get_u_fields()[2].first, "U-LLAMAS-ONE-TWO");
        EXPECT_EQ(pdu.get_u_fields()[2].second, "Unknown custom field 4");

        EXPECT_EQ(pdu.get_u_fields()[3].first, "U-AND_OTHER-CHARS");
        EXPECT_EQ(pdu.get_u_fields()[3].second, "Unknown custom field 5");

        EXPECT_EQ(pdu.get_u_fields()[4].first, "u-the-last-one");
        EXPECT_EQ(pdu.get_u_fields()[4].second, "Unknown custom field 6");
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
