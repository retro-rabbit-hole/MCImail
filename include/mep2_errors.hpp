#pragma once

#include <exception>
#include <string>
#include <unordered_map>

enum class Mep2ErrorCode {
    // 100-series: complete success, all actions performed
    Success = 100,
    Partial_Success = 101, // (not used) some parameters ignored -
                           // slave was limited in its abilities, but believes it
                           // substantially fulfilled the request

    // 200-series: intermediate success; ready for additional information
    Intermediate_Success = 200, // (not used) additional information
                                // required

    // 300-series: complete failure; cannot be performed under any conditions
    Unable_To_Perform = 300, // unknown reason
    PDU_Syntax_Error = 301,
    Protocol_Violation = 302, // request is out-of-sequence
    Malformed_Data = 303,
    Unimplemented_Function = 304,
    Partial_Failure = 305, // (not used) some parameters ignored -
                           // slave was limited in its abilities, and believes it did
                           // NOT substantially fulfilled the request
    Envelope_Problem = 310,
    Envelope_No_Data = 311,
    Envelope_No_To = 312,
    Master_Must_Term_Permanent = 399,

    // 400-series: temporary failure; cannot perform now
    System_Error = 400, // unknown reason
    Insufficient_Space = 401,
    Master_Should_Turn = 402,
    Checksum_Error = 403,
    System_Unavailable = 404,
    Batch_Mode_Unavailable = 405,
    Account_Unknown = 406,
    Account_In_Use = 407,
    Connections_Busy = 408,
    Timeout = 409,
    Too_Many_Checksum_Errors = 498, // aborting connection
    Master_Must_Term_Temporary = 499,
};

static const std::unordered_map<Mep2ErrorCode, const std::string> Mep2ErrorMessages = {
    // 100-series: complete success, all actions performed
    {Mep2ErrorCode::Success, "Request performed successfully"},
    {Mep2ErrorCode::Partial_Success, "Partial success"},

    // 200-series: intermediate success; ready for additional information
    {Mep2ErrorCode::Intermediate_Success, "Intermediate success"},

    // 300-series: complete failure; cannot be performed under any conditions
    {Mep2ErrorCode::Unable_To_Perform, "Unable to perform"},
    {Mep2ErrorCode::PDU_Syntax_Error, "PDU syntax error"},
    {Mep2ErrorCode::Protocol_Violation, "Protocol violation"},
    {Mep2ErrorCode::Malformed_Data, "Malformed data"},
    {Mep2ErrorCode::Unimplemented_Function, "Unimplemented function"},
    {Mep2ErrorCode::Partial_Failure, "Partial failure"},
    {Mep2ErrorCode::Envelope_Problem, "At least one problem within envelope"},
    {Mep2ErrorCode::Envelope_No_Data, "No envelope data received"},
    {Mep2ErrorCode::Envelope_No_To, "At least one To: recipient required"},
    {Mep2ErrorCode::Master_Must_Term_Permanent, "Master must issue /TERM request"},

    // 400-series: temporary failure; cannot perform now
    {Mep2ErrorCode::System_Error, "System error"},
    {Mep2ErrorCode::Insufficient_Space, "Insufficient space to perform action"},
    {Mep2ErrorCode::Master_Should_Turn, "Request for master to issue /TURN request"},
    {Mep2ErrorCode::Checksum_Error, "Checksum error"},
    {Mep2ErrorCode::System_Unavailable, "System not currently available"},
    {Mep2ErrorCode::Batch_Mode_Unavailable, "Batch mode not available now"},
    {Mep2ErrorCode::Account_Unknown, "Account unknown"},
    {Mep2ErrorCode::Account_In_Use, "Account already in use"},
    {Mep2ErrorCode::Connections_Busy, "All connections to MCI Mail currently busy"},
    {Mep2ErrorCode::Timeout, "Timeout has occurred"},
    {Mep2ErrorCode::Too_Many_Checksum_Errors, "Too many checksum errors"},
    {Mep2ErrorCode::Master_Must_Term_Temporary, "Master must issue /TERM request"}};

class Mep2Error : public std::exception {
  public:
    Mep2Error() = delete;
    Mep2Error(const Mep2ErrorCode code) : _code(code) {}
    Mep2Error(const Mep2ErrorCode code, const std::string& context)
        : _code(code), _context_message(Mep2ErrorMessages.at(_code) + ": " + context) {}

    Mep2ErrorCode code() const { return _code; }

    const char* what() const noexcept override {
        if (_context_message.empty()) {
            return Mep2ErrorMessages.at(_code).c_str();
        } else {
            return _context_message.c_str();
        }
    }

  private:
    const Mep2ErrorCode _code;
    std::string _context_message;
};

#define Mep2Exception(name, code)                                                                  \
    class name : public Mep2Error {                                                                \
      public:                                                                                      \
        name() : Mep2Error(code) {}                                                                \
        name(const std::string& context) : Mep2Error(code, context) {}                             \
    }

Mep2Exception(UnableToPerformError, Mep2ErrorCode::Unable_To_Perform);
Mep2Exception(PduSyntaxError, Mep2ErrorCode::PDU_Syntax_Error);
Mep2Exception(PduMalformedDataError, Mep2ErrorCode::Malformed_Data);
Mep2Exception(PduEnvelopeDataError, Mep2ErrorCode::Envelope_Problem);
Mep2Exception(PduNoEnvelopeDataError, Mep2ErrorCode::Envelope_No_Data);
Mep2Exception(PduToRequiredError, Mep2ErrorCode::Envelope_No_To);
Mep2Exception(PduChecksumError, Mep2ErrorCode::Checksum_Error);

#undef Mep2Exception
