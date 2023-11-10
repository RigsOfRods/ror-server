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
#include "prerequisites.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

struct QueueEntry {
    RoRnet::Header header;
    char data[RORNET_MAX_MESSAGE_LENGTH];
};


class Broadcaster {
public:
    static const int QUEUE_SOFT_LIMIT = 100;
    static const int QUEUE_HARD_LIMIT = 300;

    enum class ThreadState
    {
        NOT_RUNNING,      //!< Initial/terminal state - thread not running.
        RUNNING,          //!< Thread running.
        STOP_REQUESTED,   //!< Thread running.
    };

    Broadcaster(Sequencer *sequencer);
    ~Broadcaster();

    void Start(Client* client);
    void Stop();

    void QueuePacket(RoRnet::Header header, const char *payload);
    bool IsDroppingPackets() const { return m_is_dropping_packets; }

private:
    void  ThreadMain();
    ThreadState ThreadWaitForMessage(QueueEntry& out_message);
    bool  ThreadTransmitMessage(QueueEntry const& message); //!< Returns false on error.

    // Thread context
    std::thread              m_thread;
    ThreadState              m_thread_state = ThreadState::NOT_RUNNING;
    std::mutex               m_mutex;

    // Queue
    std::deque<QueueEntry>   m_msg_queue;
    std::condition_variable  m_queue_cond;

    // Broadcaster state
    Sequencer*               m_sequencer = nullptr;
    Client*                  m_client = nullptr;
    bool                     m_is_dropping_packets = false;
    int                      m_packet_drop_counter = 0;
    int                      m_packet_good_counter = 0;
};
