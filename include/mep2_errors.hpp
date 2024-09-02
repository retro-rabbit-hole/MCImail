#ifndef INCLUDE_MEP2_ERRORS_HPP_
#define INCLUDE_MEP2_ERRORS_HPP_

#include <exception>
#include <string>

class Mep2Error : public std::exception {
  public:
    Mep2Error(int code, const std::string& message) : _code(code), _message(message) {}

    int code() const { return _code; }
    const char* what() const noexcept override { return _message.c_str(); }

  private:
    int _code;
    const std::string _message;
};

class UnableToPerformError : public Mep2Error {
  public:
    UnableToPerformError() : Mep2Error(300, "Unable to perform") {}
    UnableToPerformError(const std::string& context)
        : Mep2Error(300, "Unable to perform: " + context) {}
};

class PduSyntaxError : public Mep2Error {
  public:
    PduSyntaxError() : Mep2Error(301, "PDU syntax error") {}
    PduSyntaxError(const std::string& context) : Mep2Error(301, "PDU syntax error: " + context) {}
};

class PduMalformedDataError : public Mep2Error {
  public:
    PduMalformedDataError() : Mep2Error(303, "Malformed data") {}
    PduMalformedDataError(const std::string& context)
        : Mep2Error(303, "Malformed data: " + context) {}
};

class PduEnvelopeDataError : public Mep2Error {
  public:
    PduEnvelopeDataError() : Mep2Error(310, "At least one problem within envelope") {}
    PduEnvelopeDataError(const std::string& context)
        : Mep2Error(310, "At least one problem within envelope: " + context) {}
};

class PduNoEnvelopeDataError : public Mep2Error {
  public:
    PduNoEnvelopeDataError() : Mep2Error(311, "No envelope data received") {}
};

class PduToRequiredError : public Mep2Error {
  public:
    PduToRequiredError() : Mep2Error(312, "At least one To: recipient required") {}
};

class PduChecksumError : public Mep2Error {
  public:
    PduChecksumError() : Mep2Error(403, "Checksum error") {}
    PduChecksumError(const std::string& context) : Mep2Error(403, "Checksum error: " + context) {}
};
#endif /* INCLUDE_MEP2_ERRORS_HPP_ */
