/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright (c) 2019 Petr Ohlidal

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with "Rigs of Rods Server".
If not, see <http://www.gnu.org/licenses/>.
*/

#include "client.h"
#include "sequencer.h"

#include <event2/event.h>

Client::Client(Sequencer* seq, kissnet::tcp_socket sock)
    : m_sequencer(seq)
{
    Utils::ZeroOut(m_incoming_msg);
    Utils::ZeroOut(m_user_info);
    m_socket = std::move(sock);
}

void Client::ProcessReceived()
{
    // Thanks to libevent's watermarks we always know buffer has enough data

    if (m_incoming_msg.command == RoRnet::MSG2_NONE)
    {
        // We received a new header
        ::bufferevent_read(m_buffer_event, &m_incoming_msg, sizeof(RoRnet::Header)); // Drain buffer
        ::bufferevent_setwatermark(m_buffer_event, EV_READ, m_incoming_msg.size, 0); // Wait for payload
    }
    else if (m_incoming_msg.command >= RoRnet::MSG2_HELLO &
             m_incoming_msg.command <= RoRnet::MSG2_STREAM_DATA_DISCARDABLE)
    {
        // We received valid payload
        char payload[RORNET_MAX_MESSAGE_LENGTH] = {};
        ::bufferevent_read(m_buffer_event, &payload, m_incoming_msg.size); // Drain buffer
        
        // Process message
        m_sequencer->queueMessage(
            m_user_info.uniqueid, m_incoming_msg.command,
            m_incoming_msg.streamid, payload, m_incoming_msg.size);                 

        // Wait for next header
        Utils::ZeroOut(m_incoming_msg);
        ::bufferevent_setwatermark(m_buffer_event, EV_READ, sizeof(RoRnet::Header), 0);
    }
    else
    {
        // We received bad data
        m_sequencer->QueueClientForDisconnect(m_user_info.uniqueid, "Invalid messge from client");
    }
}

// -------------------- Callbacks (static) -------------------- //

void Client::BufReadCallback(::bufferevent* bev, void* ctx)
{
    static_cast<Client*>(ctx)->ProcessReceived();
}

void Client::BufWriteCallback(::bufferevent* bev, void* ctx)
{
    
}

void Client::BufEventCallback(::bufferevent* bev, short events, void* ctx);
