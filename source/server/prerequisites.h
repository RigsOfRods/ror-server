
#pragma once

// Forward declarations
class Client;

class Dispatcher;

class Sequencer;

class Broadcaster;

class Listener;

class UserAuth;

class ScriptEngine;

namespace Http {
    class Response;
}

// KISSnet: multiplatform sockets abstraction layer (C++17)
#include <kissnet.hpp>
#ifdef SendMessage // Leaking Win32 defines
#   undef SendMessage
#endif

// Common includes
#include "config.h"
#include "logger.h"
#include "utils.h"
#include "messaging.h"
