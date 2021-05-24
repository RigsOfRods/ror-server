/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "rornet.h" // For RORNET_MAX_MESSAGE_LENGTH
#include "prerequisites.h"

#include <mutex>
#include <thread>

/// Provides a receiver thread for a single client.
class Receiver {
public:
    enum class ThreadState
    {
        NOT_RUNNING,      // Initial/terminal state - thread not running, nothing waiting on socket.
        RUNNING,          // Thread running, may be blocked by socket.
        STOP_REQUESTED,   // Thread running, may be blocked by socket.
    };

    Receiver(Sequencer *sequencer);
    ~Receiver();

    void Start(Client* client);
    void Stop();
    ThreadState GetThreadState();

private:
    void ThreadMain();
    bool ThreadReceiveMessage(); //!< @return false if thread should be stopped, true to continue.
    bool ThreadReceiveHeader(); //!< @return false if thread should be stopped, true to continue.
    bool ThreadReceivePayload(); //!< @return false if thread should be stopped, true to continue.

    Sequencer*  m_sequencer = nullptr; // global
    Client*     m_client = nullptr;    // data owner
    std::mutex  m_mutex;
    ThreadState m_thread_state = ThreadState::NOT_RUNNING;
    std::thread m_thread;

    // Received data buffers -- Keep here to be allocated on heap (along with Client)
    RoRnet::Header m_recv_header;
    char           m_recv_payload[RORNET_MAX_MESSAGE_LENGTH];
};

