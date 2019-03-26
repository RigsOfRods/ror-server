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

#include "rornet.h"
#include "mutexutils.h"
#include "prerequisites.h"

#include <atomic>
#include <pthread.h>
#include <deque>

class SWInetSocket;

class Sequencer;

struct queue_entry_t {
    int type;
    int uid;
    unsigned int streamid;
    unsigned int datalen;
    char data[RORNET_MAX_MESSAGE_LENGTH];
};

void *StartBroadcasterThread(void *);

class Broadcaster {
    friend void *StartBroadcasterThread(void *);

public:
    static const int QUEUE_SOFT_LIMIT = 100;
    static const int QUEUE_HARD_LIMIT = 300;

    Broadcaster(Sequencer *sequencer);

    void Start(Client* client);

    void Stop();

    void QueueMessage(int msg_type, int client_id, unsigned int streamid, unsigned int payload_len, const char *payload);

    bool IsDroppingPackets() const { return m_is_dropping_packets; }

private:
    void Thread();

    Sequencer *m_sequencer;
    pthread_t m_thread;
    Mutex m_queue_mutex;
    Condition m_queue_cond;
    Client* m_client;
    std::atomic<bool> m_keep_running;
    bool m_is_dropping_packets;
    int  m_packet_drop_counter;
    int  m_packet_good_counter;

    std::deque<queue_entry_t> m_msg_queue;
};
