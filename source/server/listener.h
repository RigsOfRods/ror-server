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

#include "SocketW.h"
#include "prerequisites.h"

#include <mutex>
#include <thread>

class Listener {
public:
    enum State {
        NOT_RUNNING,      //!< Initial/terminal state - thread not running, nothing waiting on socket.
        RUNNING,          //!< Thread running, may be blocked by socket.
        STOP_REQUESTED,   //!< Thread running, may be blocked by socket.
        THREAD_STOPPED,   //!< Thread ended, possibly on error - pending cleanup.
    };

    Listener(Sequencer *sequencer);
    ~Listener();

    bool                Start();                //!< Setup listening socket and start thread. Return true on success.
    void                Stop();

private:
    void                Thread();
    void                ThreadHandleConnection(SWInetSocket* ts);

    State               m_state = NOT_RUNNING;
    std::mutex          m_mutex;
    std::thread         m_thread;
    Sequencer*          m_sequencer = nullptr;
    SWInetSocket        m_listen_socket;
};

