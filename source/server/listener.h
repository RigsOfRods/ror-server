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

#include <pthread.h>
#include "SocketW.h"

class Sequencer;

class Listener
{
private:
    pthread_t thread;
    int lport;
    bool running;
    SWInetSocket listSocket;
    pthread_mutex_t* m_ready_mtx;
    pthread_cond_t* m_ready_cond;
    int* m_ready_value;
    Sequencer* m_sequencer;

    /// Signals the main thread that we're ready to listen for connections.
    void signalReady();
public:
    Listener(Sequencer* sequencer, int port, pthread_mutex_t* ready_mtx, pthread_cond_t* ready_cond, int* ready_value);
    ~Listener(void);
    void threadstart();
};

