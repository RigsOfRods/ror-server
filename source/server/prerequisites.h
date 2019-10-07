
#pragma once

// Forward declarations
class Client;

class Sequencer;

class Broadcaster;

class Receiver;

class Listener;

class UserAuth;

class ScriptEngine;

namespace Http {
    class Response;
}

// Common includes
#include "config.h"
#include "logger.h"
#include "utils.h"

// KISSnet: multiplatform sockets abstraction layer (C++17)
#include <kissnet.hpp>

#ifdef SendMessage // Leaking Win32 defines
#   undef SendMessage
#endif
