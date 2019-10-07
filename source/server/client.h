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

#include "prerequisites.h"
#include "rornet.h" // For RORNET_MAX_MESSAGE_LENGTH

#include <event2/bufferevent.h>

/// Sends/recvs all client data, updates user status
class Client
{
public:
    Client(Sequencer* seq, kissnet::tcp_socket sock);

// Messaging:
    void ProcessReceived();

// Callbacks:
    static void BufReadCallback(::bufferevent* bev, void* ctx);
    static void BufWriteCallback(::bufferevent* bev, void* ctx);
    static void BufEventCallback(::bufferevent* bev, short events, void* ctx);

private:
    kissnet::tcp_socket  m_socket;
    ::bufferevent*       m_buffer_event = nullptr;
    RoRnet::UserInfo     m_user_info;
    RoRnet::Header       m_incoming_msg;
    Sequencer*           m_sequencer = nullptr;
};