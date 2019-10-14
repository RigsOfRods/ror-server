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
#include "dispatcher.h"
#include "sequencer.h"

#include <event2/event.h>

Client::Client(Sequencer* seq, kissnet::tcp_socket sock)
    : m_sequencer(seq)
{
    Utils::ZeroOut(m_incoming_msg);
    Utils::ZeroOut(this->user);
    m_socket = std::move(sock);
}

Client::~Client()
{
    ::bufferevent_free(m_buffer_event);
    m_socket.close();
}

bool Client::SubscribeForEvents(Dispatcher* dispatcher)
{
    assert(m_socket.is_valid());

    // Create event-driven buffer
    m_buffer_event = ::bufferevent_socket_new(
        dispatcher->GetEventBase(), m_socket.get_native(), 0);
    if (!m_buffer_event)
    {
        Logger::Log(LOG_ERROR, "Failed to register client with Dispatcher");
        return false;
    }

    // Attach buffer callbacks
    ::bufferevent_setcb(m_buffer_event,
        &Client::BufReadCallback,
        &Client::BufWriteCallback,
        &Client::BufEventCallback,
        this);

    // Set buffer to wait for header
    Utils::ZeroOut(m_incoming_msg);
    ::bufferevent_setwatermark(m_buffer_event, EV_READ, sizeof(RoRnet::Header), 0);

    m_sequencer->IntroduceNewClientToAllVehicles(this);

    // Start receiving data
    m_socket.set_non_blocking();
    ::bufferevent_enable(m_buffer_event, EV_READ|EV_WRITE);

    return true;
}

void Client::ProcessReceivedData()
{
    // Thanks to libevent's watermarks we always know buffer has enough data

    if (m_incoming_msg.command == RoRnet::MSG2_NONE)
    {
        // We received a new header
        ::bufferevent_read(m_buffer_event, &m_incoming_msg, sizeof(RoRnet::Header)); // Drain buffer
        ::bufferevent_setwatermark(m_buffer_event, EV_READ, m_incoming_msg.size, 0); // Wait for payload
    }
    else if (m_incoming_msg.command >= RoRnet::MSG2_HELLO &&
             m_incoming_msg.command <= RoRnet::MSG2_STREAM_DATA_DISCARDABLE)
    {
        // We received valid payload
        char payload[RORNET_MAX_MESSAGE_LENGTH] = {};
        ::bufferevent_read(m_buffer_event, &payload, m_incoming_msg.size); // Drain buffer
        
        // Process message
        m_sequencer->queueMessage(
            this->user.uniqueid, m_incoming_msg.command,
            m_incoming_msg.streamid, payload, m_incoming_msg.size);                 

        // Wait for next header
        Utils::ZeroOut(m_incoming_msg);
        ::bufferevent_setwatermark(m_buffer_event, EV_READ, sizeof(RoRnet::Header), 0);
    }
    else
    {
        // We received bad data
        m_sequencer->QueueClientForDisconnect(this->user.uniqueid, "Invalid messge from client");
    }
}

void Client::QueueMessage(int msg_type, int client_id, unsigned int stream_id, unsigned int payload_len,
                          const char *payload) {
    m_broadcaster.QueueMessage(msg_type, client_id, stream_id, payload_len, payload);
    if (m_buffer_event && m_sender_idle)
    {
        this->TransmitQueuedData();
    }
}

void Client::TransmitQueuedData()
{
    m_sender_idle = true;
    queue_entry_t msg;
    while (m_broadcaster.PopMessageFromQueue(msg))
    {
        m_sender_idle = false;

        // Write header
        RoRnet::Header head;
        head.command  = msg.type;
        head.source   = msg.uid;
        head.size     = msg.datalen;
        head.streamid = msg.streamid;
        int res = ::bufferevent_write(m_buffer_event, &head, sizeof(RoRnet::Header));
        if (res != 0)
        {
            Logger::Log(LOG_WARN,
                "Client %d: Error sending msg header; bufferevent_write() return code %d",
                this->user.uniqueid, res);
        }

        // Write payload
        res = ::bufferevent_write(m_buffer_event, &msg.data, msg.datalen);
        if (res != 0)
        {
            Logger::Log(LOG_WARN,
                "Client %d: Error sending msg payload; bufferevent_write() return code %d",
                this->user.uniqueid, res);
        }
    }
}

void Client::HandleSocketEvent(short events)
{
    if (events & BEV_EVENT_ERROR)
    {
        const char* error_msg = ::evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
        Logger::Log(LOG_ERROR, "Client %d (%s) connection failure: %s",
            this->user.uniqueid, this->user.username, error_msg);

        std::string reason_msg = std::string("Connection failure: ") + error_msg;
        m_sequencer->QueueClientForDisconnect(this->user.uniqueid, reason_msg.c_str(), true);
    }
    else if (events & BEV_EVENT_EOF)
    {
        Logger::Log(LOG_INFO, "Client %d (%s) disconnected", this->user.uniqueid, this->user.username);
        m_sequencer->QueueClientForDisconnect(this->user.uniqueid, "Connection closed", true);
    }
}

// -------------------- Callbacks (static) -------------------- //

void Client::BufReadCallback(::bufferevent* bev, void* ctx)
{
    static_cast<Client*>(ctx)->ProcessReceivedData();
}

void Client::BufWriteCallback(::bufferevent* bev, void* ctx)
{
    static_cast<Client*>(ctx)->TransmitQueuedData();
}

void Client::BufEventCallback(::bufferevent* bev, short events, void* ctx)
{
    static_cast<Client*>(ctx)->HandleSocketEvent(events);
}
