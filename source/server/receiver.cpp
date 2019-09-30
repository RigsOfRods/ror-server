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

Receiver::Receiver(Sequencer *sequencer, Client* client) :
        m_client(client),
        m_sequencer(sequencer)
{
}

Receiver::~Receiver()
{
    try
    {
        if (m_thread.joinable())
            m_thread.join();
    }
    catch (std::exception& e)
    {
        Logger::Log(LOG_ERROR, "Failed to stop receiver thread: %s", e.what());
    }
}

void Receiver::StartThread()
{
    m_thread = std::thread(&Receiver::Thread, this);
}

void Receiver::Thread()
{
    Logger::Log(LOG_DEBUG, "Started receiver thread %d owned by user ID %d", ThreadID::getID(), m_client->GetUserId());

    for (;;)
    {
        // Block until a message arrives
        RoRnet::Header header;
        const int recv_result = Messaging::ReceiveMessage(m_client->GetSocket(), header, m_buffer);
        if (recv_result != 0 || !m_client->GetSocket().is_valid())
        {
            m_sequencer->QueueClientForDisconnect(m_client->GetUserId(), "Game connection closed");
            return;
        }

        if (header.command != RoRnet::MSG2_STREAM_DATA &&
            header.command != RoRnet::MSG2_STREAM_DATA_DISCARDABLE) {
            Logger::Log(LOG_VERBOSE, "got message: type: %d, source: %d:%d, len: %d",
                header.command, header.source, header.streamid, header.size);
        }

        if (header.command < 1000 || header.command > 1050) {
            m_sequencer->QueueClientForDisconnect(m_client->GetUserId(), "Protocol error 3");
            return;
        }

        m_sequencer->ProcessMessage(m_client, header, m_buffer);
    }

    Logger::Log(LOG_DEBUG, "Receiver thread %d (user ID %d) exits", ThreadID::getID(), m_client->GetUserId());
}
