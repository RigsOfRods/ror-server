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

#include "broadcaster.h"

#include "logger.h"
#include "messaging.h"
#include "SocketW.h"
#include "sequencer.h"

#include <cassert>
#include <cstring>
#include <map>
#include <algorithm>

Broadcaster::Broadcaster(Sequencer *sequencer) :
    m_sequencer(sequencer) {
}


Broadcaster::~Broadcaster() {
}


void Broadcaster::Start(Client* client) {
    std::lock_guard<std::mutex> scoped_lock(m_mutex);

    m_client = client;
    m_is_dropping_packets = false;
    m_packet_drop_counter = 0;
    m_packet_good_counter = 0;
    m_msg_queue.clear();

    m_thread = std::thread(&Broadcaster::ThreadMain, this);
    m_thread_state = ThreadState::RUNNING;
}


void Broadcaster::Stop() {
    {
        std::lock_guard<std::mutex> scoped_lock(m_mutex);
        switch (m_thread_state) {
        case ThreadState::RUNNING:
            Logger::Log(LOG_DEBUG, "Broadcaster::Stop() (client_id %d) Thread state is RUNNING -> stopping", m_client->GetUserId());
            m_thread_state = ThreadState::STOP_REQUESTED;
            break;
        case ThreadState::NOT_RUNNING:
            Logger::Log(LOG_DEBUG, "Broadcaster::Stop() (client_id %d) Thread state is NOT_RUNNING -> nothing to do", m_client->GetUserId());
            return; // We're done here.
        case ThreadState::STOP_REQUESTED:
            Logger::Log(LOG_DEBUG, "Broadcaster::Stop() (client_id %d) Thread state is STOP_REQUESTED -> nothing to do", m_client->GetUserId());
            return; // We're done here.
        }
    }

    m_queue_cond.notify_one(); // Unblock the thread.
    m_thread.join(); // Wait for thread to exit.

    {
        std::lock_guard<std::mutex> scoped_lock(m_mutex);
        m_thread_state = ThreadState::NOT_RUNNING;
    }
}


void Broadcaster::ThreadMain() {
    Logger::Log(LOG_DEBUG, "Started broadcaster thread (client_id %d)", m_client->GetUserId());

    bool exit_loop = false;
    while (!exit_loop) {
        QueueEntry message;
        ThreadState state = this->ThreadWaitForMessage(message);

        if (state == ThreadState::STOP_REQUESTED) {
            Logger::Log(LOG_DEBUG, "Broadcaster thread (client_id %d) was requested to stop", m_client->GetUserId());
            // Synchronously send all the remaining messages and exit.
            std::lock_guard<std::mutex> scoped_lock(m_mutex);
            while (!m_msg_queue.empty() && this->ThreadTransmitMessage(m_msg_queue.front())) {
                m_msg_queue.pop_front();
            }
            exit_loop = true;
        } else {
            if (!this->ThreadTransmitMessage(message)) {
                m_sequencer->disconnectClient(m_client->GetUserId(), "Broadcaster: Send error", true, true);
                exit_loop = true;
            }
        }
    }

    Logger::Log(LOG_DEBUG, "Broadcaster thread (client_id %d) exits", m_client->GetUserId());
}


Broadcaster::ThreadState Broadcaster::ThreadWaitForMessage(QueueEntry& out_message) {
    std::unique_lock<std::mutex> uni_lock(m_mutex); // Scoped
    if (m_msg_queue.empty()) {
        m_queue_cond.wait(uni_lock);
    }
    if (!m_msg_queue.empty()) {
        out_message = m_msg_queue.front();
        m_msg_queue.pop_front();
    }
    return m_thread_state;
}


bool Broadcaster::ThreadTransmitMessage(QueueEntry const& msg) {
    int type = msg.type;
    if (type == RoRnet::MSG2_INVALID)
        return true; // No error.
    if (type == RoRnet::MSG2_STREAM_DATA_DISCARDABLE)
        type = RoRnet::MSG2_STREAM_DATA;

    int res = Messaging::SendMessage(m_client->GetSocket(), type, msg.uid, msg.streamid, msg.datalen, msg.data);
    return res == 0;
}


void Broadcaster::QueueMessage(int type, int uid, unsigned int streamid, unsigned int len, const char *data) {
    QueueEntry msg;
    msg.type = (RoRnet::MessageType)type;
    msg.uid = uid;
    msg.streamid = streamid;
    msg.datalen = len;
    std::memcpy(msg.data, data, len);

    {
        std::lock_guard<std::mutex> scoped_lock(m_mutex);
        if (m_msg_queue.empty()) {
            m_packet_drop_counter = 0;
            m_is_dropping_packets = (++m_packet_good_counter > 3) ? false : m_is_dropping_packets;
        } else if (type == RoRnet::MSG2_STREAM_DATA_DISCARDABLE) {
            auto search = std::find_if(m_msg_queue.begin(), m_msg_queue.end(), [&](const QueueEntry& m)
                    { return m.type == RoRnet::MSG2_STREAM_DATA_DISCARDABLE && m.uid == uid && m.streamid == streamid; });
            if (search != m_msg_queue.end()) {
                // Found outdated discardable streamdata -> replace it
                (*search) = msg;
                m_packet_good_counter = 0;
                m_is_dropping_packets = (++m_packet_drop_counter > 3) ? true : m_is_dropping_packets;
                Messaging::StatsAddOutgoingDrop(sizeof(RoRnet::Header) + msg.datalen); // Statistics
                return;
            }
        }
        m_msg_queue.push_back(msg);
    }

    m_queue_cond.notify_one();
}

