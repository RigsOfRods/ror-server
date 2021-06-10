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
    enum State {
        NOT_RUNNING,      // Initial/terminal state - thread not running, nothing waiting on socket.
        RUNNING,          // Thread running, may be blocked by socket.
        STOP_REQUESTED,   // Thread running, may be blocked by socket.
    };

    Receiver(Sequencer *sequencer);
    ~Receiver();

    void Start(Client* client);
    void Stop();

private:
    void Thread();
    void ThreadStartup();
    bool ThreadReceiveMessage(int *out_type, int *out_source, unsigned int *out_streamid, unsigned int *out_payload_len); //!< @return false to exit thread.
    bool ThreadProcessMessage(int type, int source, unsigned streamid, unsigned payload_len); //!< @return false to exit thread.
    void ThreadShutdown();

    Sequencer*  m_sequencer = nullptr; // global
    Client*     m_client = nullptr;    // data owner
    char        m_dbuffer[RORNET_MAX_MESSAGE_LENGTH] = {}; // Keep here to be allocated on heap (along with Client)
    std::mutex  m_mutex;
    State       m_state = NOT_RUNNING;
    std::thread m_thread;
};

