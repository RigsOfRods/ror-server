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

#include <assert.h>

Receiver::Receiver(Sequencer *sequencer):
    m_sequencer(sequencer)
{
}

Receiver::~Receiver() {
    assert(m_state == NOT_RUNNING);
}

void Receiver::Start(Client* client) {
    std::lock_guard<std::mutex> lock(m_mutex); // Scoped

    assert(m_state == NOT_RUNNING);
    m_client = client;
    m_thread = std::thread(&Receiver::Thread, this);
    m_state = RUNNING;
}

void Receiver::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Scoped
        if (m_state != RUNNING)
            return;
        m_state = STOP_REQUESTED;
    }

    m_thread.join();

    {
        std::lock_guard<std::mutex> lock(m_mutex); // Scoped
        m_state = NOT_RUNNING;
    }
}

void Receiver::Thread() {
    Logger::Log(LOG_DEBUG, "Started receiver thread %u owned by user ID %d", ThreadID::getID(), m_client->GetUserId());

    m_sequencer->sendMOTD(m_client->GetUserId()); // send MOTD

    m_client->GetSocket()->set_timeout((Uint32)60, 0); // 60sec

    m_client->SetReceiveData(true);
    Logger::Log(LOG_VERBOSE, "UID %d is switching to FLOW", m_client->GetUserId());

    while (true) {
        int type;
        int source;
        unsigned int streamid;
        unsigned int len;
        if (Messaging::ReceiveMessage(m_client->GetSocket(), &type, &source, &streamid, &len, m_dbuffer,
                                      RORNET_MAX_MESSAGE_LENGTH)) {
            m_sequencer->QueueClientForDisconnect(m_client->GetUserId(), "Game connection closed");
            break;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex); // Scoped
            if (m_state == STOP_REQUESTED)
            {
                break;
            }
        }

        if (type != RoRnet::MSG2_STREAM_DATA && type != RoRnet::MSG2_STREAM_DATA_DISCARDABLE) {
            Logger::Log(LOG_VERBOSE, "got message: type: %d, source: %d:%u, len: %u", type, source, streamid, len);
        }

        if (type < 1000 || type > 1050) {
            m_sequencer->QueueClientForDisconnect(m_client->GetUserId(), "Protocol error 3");
            break;
        }
        m_sequencer->queueMessage(m_client->GetUserId(), type, streamid, m_dbuffer, len);
    }

    Logger::Log(LOG_DEBUG, "Receiver thread %u (user ID %d) exits", ThreadID::getID(), m_client->GetUserId());
}
