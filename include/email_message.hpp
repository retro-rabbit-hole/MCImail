#pragma once

#define CAPNP_LITE 1
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/std/iostream.h>

#include "mep2_pdu.hpp"

class EmailMessage {
  public:
  private:
    EnvPdu _envelope;
};