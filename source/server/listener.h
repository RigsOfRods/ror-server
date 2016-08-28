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
#include "mutexutils.h"
#include "prerequisites.h"

#include <pthread.h>
#include <atomic>

class Listener
{
private:
    pthread_t             m_thread;
    SWInetSocket          m_listen_socket;
    int                   m_listen_port;
    Threading::SimpleCond m_ready_cond;
    Sequencer*            m_sequencer;
    std::atomic_bool      m_thread_shutdown;
public:
    Listener(Sequencer* sequencer, int port);
    ~Listener(void);
    void threadstart();
    bool Initialize();
    bool WaitUntilReady();
    void Shutdown();
};

