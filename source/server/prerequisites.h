
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

#include "logger.h"
#include "rornet.h"

#include <kissnet.hpp>
#ifdef SendMessage // Kissnet leaks Win32 defines
#   undef SendMessage
#endif
