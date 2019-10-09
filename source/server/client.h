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

#pragma once

#include "broadcaster.h"
#include "prerequisites.h"
#include "rornet.h" // For RORNET_MAX_MESSAGE_LENGTH

#include <event2/bufferevent.h>
#include <map>

/// Sends/recvs all client data, updates user status
class Client
{
public:
    Client(Sequencer* seq, kissnet::tcp_socket sock);
    ~Client();
    void SubscribeForEvents(::bufferevent* bev);

// Legacy getters (to be refactored)
    kissnet::tcp_socket& GetSocket() { return m_socket; }
    std::string GetIpAddress() { return m_socket.get_recv_endpoint().address; }
    int GetUserId() const { return static_cast<int>(user.uniqueid); }
    bool IsBroadcasterDroppingPackets() const { return m_broadcaster.IsDroppingPackets(); }

// Messaging:
    void ProcessReceivedData();
    void TransmitQueuedData();
    void QueueMessage(int msg_type, int client_id, unsigned int stream_id, unsigned int payload_len, const char *payload);

// Callbacks:
    static void BufReadCallback(::bufferevent* bev, void* ctx);
    static void BufWriteCallback(::bufferevent* bev, void* ctx);
    static void BufEventCallback(::bufferevent* bev, short events, void* ctx);

// Public vars (to be refactored)
    RoRnet::UserInfo user;
    int drop_state;             // dropping outgoing packets?
    std::map<unsigned int, RoRnet::StreamRegister> streams;
    std::map<unsigned int, stream_traffic_t> streams_traffic;

private:
    kissnet::tcp_socket  m_socket;
    ::bufferevent*       m_buffer_event = nullptr;
    RoRnet::Header       m_incoming_msg;
    Sequencer*           m_sequencer = nullptr;
    Broadcaster          m_broadcaster; // Legacy
};
