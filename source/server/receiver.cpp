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

#include "receiver.h"

#include "SocketW.h"
#include "sequencer.h"
#include "messaging.h"
#include "ScriptEngine.h"
#include "logger.h"

#include <cstring>
#include <cassert>


Receiver::Receiver(Sequencer *sequencer):
    m_sequencer(sequencer)
{
}

Receiver::~Receiver() {
    assert(m_thread_state == ThreadState::NOT_RUNNING);
}

void Receiver::Start(Client* client) {
    std::lock_guard<std::mutex> lock(m_mutex); // Scoped

    assert(m_thread_state == ThreadState::NOT_RUNNING);
    m_client = client;
    m_thread = std::thread(&Receiver::ThreadMain, this);
    m_thread_state = ThreadState::RUNNING;
}

void Receiver::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Scoped
        if (m_thread_state != ThreadState::RUNNING)
            return;
        m_thread_state = ThreadState::STOP_REQUESTED;
    }

    m_thread.join();

    {
        std::lock_guard<std::mutex> lock(m_mutex); // Scoped
        m_thread_state = ThreadState::NOT_RUNNING;
    }
}

Receiver::ThreadState Receiver::GetThreadState()
{
    std::lock_guard<std::mutex> lock(m_mutex); // Scoped
    return m_thread_state;
}

void Receiver::ThreadMain() {
    Logger::Log(LOG_DEBUG, "Started receiver thread (user ID %d)", m_client->GetUserId());


    m_sequencer->sendMOTD(m_client->GetUserId()); // send MOTD

    m_client->GetSocket()->set_timeout((Uint32)60, 0); // 60sec

    m_client->SetReceiveData(true);
    Logger::Log(LOG_VERBOSE, "UID %d is switching to FLOW", m_client->GetUserId());

    while (this->GetThreadState() == ThreadState::RUNNING) {
        if (!this->ThreadReceiveMessage()) {
            m_sequencer->disconnectClient(m_client->GetUserId(), "Game connection closed");
            break;
        }

        if (m_recv_header.command != RoRnet::MSG2_STREAM_DATA &&
            m_recv_header.command != RoRnet::MSG2_STREAM_DATA_DISCARDABLE) {
            Logger::Log(LOG_VERBOSE, "got message: type: %d, source: %d:%d, len: %d",
                        (int)m_recv_header.command, (int)m_recv_header.source, (int)m_recv_header.streamid, (int)m_recv_header.size);
        }

        if (m_recv_header.command < 1000u || m_recv_header.command > 1050u) {
            m_sequencer->disconnectClient(m_client->GetUserId(), "Protocol error 3");
            break;
        }

        m_sequencer->queueMessage(m_client->GetUserId(),
            (int)m_recv_header.command, m_recv_header.streamid, m_recv_payload, m_recv_header.size);
    }

    Logger::Log(LOG_DEBUG, "Receiver thread (user ID %d) exits", m_client->GetUserId());
}

bool Receiver::ThreadReceiveMessage()
{
    if (!this->ThreadReceiveHeader())
    {
        return false; // Stop thread.
    }

    if (m_recv_header.size > 0)
    {
        if (!this->ThreadReceivePayload())
        {
            return false; // Stop thread.
        }
    }

    Messaging::StatsAddIncoming((int)sizeof(RoRnet::Header) + (int)m_recv_header.size);
    return true; // Continue receiving.
}

bool Receiver::ThreadReceiveHeader() //!< @return false if thread should be stopped, true to continue.
{
    SWBaseSocket::SWBaseError error;

    std::memset((void*)&m_recv_header, 0, sizeof(RoRnet::Header));
    if (m_client->GetSocket()->frecv((char*)&m_recv_header, (int)sizeof(RoRnet::Header), &error) <= 0)
    {
        Logger::Log(LOG_WARN, "Receiver: error getting header: %s", error.get_error().c_str());
        return false; // stop thread.
    }

    if (m_recv_header.size > RORNET_MAX_MESSAGE_LENGTH)
    {
        // Oversized payload
        Logger::Log(LOG_WARN, "Receiver: payload too long: %d/ max. %d bytes", (int)m_recv_header.size, RORNET_MAX_MESSAGE_LENGTH);
        return false; // Stop thread.
    }

    return true; // continue receiving.
}

bool Receiver::ThreadReceivePayload() //!< @return false if thread should be stopped, true to continue.
{
    SWBaseSocket::SWBaseError error;

    std::memset(m_recv_payload, 0, RORNET_MAX_MESSAGE_LENGTH);
    if (m_client->GetSocket()->frecv(m_recv_payload, (int)m_recv_header.size, &error) <= 0)
    {
        Logger::Log(LOG_WARN, "Receiver: error getting payload: %s", error.get_error().c_str());
        return false; // stop thread.
    }

    return true; // continue receiving.
}
