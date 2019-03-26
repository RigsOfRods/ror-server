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

#include <atomic>
#include <pthread.h>

class SWInetSocket;

class Sequencer;

void *LaunchReceiverThread(void *);

class Receiver {
    friend void *LaunchReceiverThread(void *);

public:
    Receiver(Sequencer *sequencer);

    ~Receiver();

    void Start(Client* client);

    void Stop();

private:
    void Thread();

    pthread_t m_thread;
    Client* m_client;
    char m_dbuffer[RORNET_MAX_MESSAGE_LENGTH]; // Keep here to be allocated on heap (along with Client)
    std::atomic<bool> m_keep_running;
    Sequencer *m_sequencer;
};

