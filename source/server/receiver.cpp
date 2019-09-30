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

#include "sequencer.h"
#include "messaging.h"
#include "ScriptEngine.h"
#include "logger.h"

void *LaunchReceiverThread(void *data) {
    Receiver *receiver = static_cast<Receiver *>(data);
    receiver->Thread();
    return nullptr;
}

Receiver::Receiver(Sequencer *sequencer) :
        m_client(nullptr),
        m_sequencer(sequencer) {
}

Receiver::~Receiver() {
    this->Stop();
}

void Receiver::Start(Client* client) {
    m_client = client;
    m_keep_running.store(true);

    pthread_create(&m_thread, nullptr, LaunchReceiverThread, this);
}

void Receiver::Stop() {
    bool was_running = m_keep_running.exchange(false);
    if (was_running) {
        //TODO: how do I signal the thread to stop waiting for the socket?
        //      For now I stick to the existing solution - I just don't ~ only_a_ptr, 03/2019
        pthread_join(m_thread, nullptr);
    }
}

void Receiver::Thread() {
    Logger::Log(LOG_DEBUG, "Started receiver thread %d owned by user ID %d", ThreadID::getID(), m_client->GetUserId());

    //okay, we are ready, we can receive data frames
    m_client->SetReceiveData(true);

    //send motd
    m_sequencer->sendMOTD(m_client->GetUserId());

    Logger::Log(LOG_VERBOSE, "UID %d is switching to FLOW", m_client->GetUserId());

    while (true) {
        RoRnet::Header header;

        if (Messaging::ReceiveMessage(m_client->GetSocket(), header, m_payload_buf)) {
            m_sequencer->QueueClientForDisconnect(m_client->GetUserId(), "Game connection closed");
            break;
        }

        if (m_keep_running.load() == false)
            break;

        if (header.command != RoRnet::MSG2_STREAM_DATA &&
            header.command != RoRnet::MSG2_STREAM_DATA_DISCARDABLE) {
            Logger::Log(LOG_VERBOSE, "got message: type: %d, source: %d:%d, len: %d",
                header.command, header.source, header.streamid, header.size);
        }

        if (header.command < 1000 || header.command > 1050) {
            m_sequencer->QueueClientForDisconnect(m_client->GetUserId(), "Protocol error 3");
            break;
        }
        m_sequencer->queueMessage(m_client->GetUserId(),
            header.command, header.streamid, (char*)m_payload_buf.data(), header.size);
    }

    Logger::Log(LOG_DEBUG, "Receiver thread %d (user ID %d) exits", ThreadID::getID(), m_client->GetUserId());
}
