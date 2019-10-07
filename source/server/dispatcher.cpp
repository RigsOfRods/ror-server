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

#include "dispatcher.h"

#include <event2/buffer.h>

Dispatcher::Dispatcher(Sequencer* sequencer, MasterServer::Client& serverlist)
    : m_conn_handler(sequencer)
    , m_serverlist(serverlist)
    , m_sequencer(sequencer)
{}

void Dispatcher::Initialize()
{
    // Create libevent `base` object
    m_ev_base = ::event_base_new();
    if (!m_ev_base)
    {
        throw std::runtime_error("Fatal startup error: event_base_new() returned null");
    }

    // Set up heartbeat timer event
    m_ev_heartbeat = ::evtimer_new(m_ev_base, &Dispatcher::HeartbeatCallback, this);
    if (!m_ev_heartbeat)
    {
        throw std::runtime_error("Fatal: failed to set up heartbeat timer, event_new() returned null");
    }
    ::timeval timeout = {Config::GetHeartbeatIntervalSec(), 0};
    ::evtimer_add(m_ev_heartbeat, &timeout); // Will fire once; callback will restart it as needed.

    // Create listening socket
    m_socket = kissnet::tcp_socket(
        kissnet::endpoint("0.0.0.0", kissnet::port_t(Config::getListenPort())));
    m_socket.bind();
    m_socket.listen();

    // Set up listener event
    const int flags = LEV_OPT_LEAVE_SOCKETS_BLOCKING; // RoRServer's client-welcome logic is synchronous
    const int backlog = 0; // `listen()` already invoked on socket
    m_ev_listener = ::evconnlistener_new(
        m_ev_base, &Dispatcher::ConnAcceptCallback, this, flags, backlog, m_socket.get_native());
    if (!m_ev_listener)
    {
        throw std::runtime_error("Fatal startup error: evconnlistener_new() returned null");
    }

    ::evconnlistener_set_error_cb(m_ev_listener, &Dispatcher::ConnErrorCallback);

    // Set up stats timer
    m_ev_stats = ::event_new(
        m_ev_base, -1, EV_TIMEOUT|EV_PERSIST, &Dispatcher::StatsCallback, this);
}

void Dispatcher::RunDispatchLoop()
{
    ::event_base_dispatch(m_ev_base);
}

void Dispatcher::Shutdown()
{
    if (m_ev_heartbeat)
    {
        ::event_free(m_ev_heartbeat);
        m_ev_heartbeat = nullptr;
    }

    if (m_ev_stats)
    {
        ::event_free(m_ev_stats);
        m_ev_stats = nullptr;
    }

    if (m_ev_listener)
    {
        ::evconnlistener_free(m_ev_listener);
        m_ev_listener = nullptr;
    }

    if (m_ev_base)
    {
        ::event_base_free(m_ev_base);
        m_ev_base = nullptr;
    }

    if (m_socket.is_valid())
    {
        m_socket.close();
    }
}

void Dispatcher::HandleNewConnection(kissnet::tcp_socket socket)
{
    return m_conn_handler.HandleNewConnection(std::move(socket));
}

void Dispatcher::UpdateStats()
{
    Messaging::UpdateMinuteStats();
    m_sequencer->UpdateMinuteStats();
}

void Dispatcher::PerformHeartbeat()
{
    ::timeval next_timeout = {Config::GetHeartbeatIntervalSec(), 0};
    if (Config::getServerMode() == SERVER_LAN)
    {
        // broadcast our "i'm here" signal
        Messaging::broadcastLAN();            
    }
    else
    {
        // Send heartbeat to serverlist
        Json::Value user_list(Json::arrayValue);
        m_sequencer->GetHeartbeatUserList(user_list);
        if (!m_serverlist.SendHeatbeat(user_list))
        {
            ++m_heartbeat_failcount;
            size_t max_failcount = Config::GetHeartbeatRetryCount()+1u;
            if (m_heartbeat_failcount >= max_failcount)
            {
                Logger::Log(LOG_ERROR, "Unable to send heartbeats, exit");
                ::event_base_loopbreak(m_ev_base);
            }
            else
            {
                next_timeout.tv_sec = Config::GetHeartbeatRetrySeconds();
                Logger::Log(
                    LOG_WARN, "A heartbeat failed! Count: %u/%u, Retry in %ld sec.",
                    m_heartbeat_failcount, max_failcount, next_timeout);
            }
        }
        else
        {
            Logger::Log(LOG_VERBOSE, "Heartbeat sent OK");
            m_heartbeat_failcount = 0;
        }
    }

    // Schedule next heartbeat
    ::event_add(m_ev_heartbeat, &next_timeout);
}

bool Dispatcher::RegisterClient(Client* client)
{
    // Create event-driven buffer
    ::bufferevent* buff_ev = ::bufferevent_socket_new(
        m_ev_base, client->GetSocket().get_native(), 0);
    if (!buff_ev)
    {
        Logger::Log(LOG_ERROR, "Failed to register client with Dispatcher");
        return false;
    }

    // Attach buffer callbacks
    ::bufferevent_setcb(buff_ev,
        &Dispatcher::BufReadCallback,
        &Dispatcher::BufWriteCallback,
        &Dispatcher::BufEventCallback,
        client);

    // Wait until whole header is buffered
    ::bufferevent_setwatermark(buff_ev, EV_READ, sizeof(RoRnet::Header), 0);
}

void Dispatcher::RemoveClient(Client* client)
{
    
}

// -------------------- Callbacks (static) -------------------- //

void Dispatcher::ConnAcceptCallback(::evconnlistener* listener,
                                ::evutil_socket_t sock,
                                sockaddr* addr, int socklen, void* ptr)
{
    kissnet::tcp_socket socket(sock, addr);
    static_cast<Dispatcher*>(ptr)->HandleNewConnection(std::move(socket));
}

void Dispatcher::ConnErrorCallback(::evconnlistener* listener, void* ptr)
{
    const int err_code = EVUTIL_SOCKET_ERROR();
    Logger::Log(LOG_ERROR,
        "Error listening for connections, shutting down! Code: %d, description: %s",
        err_code, ::evutil_socket_error_to_string(err_code));

    ::event_base_loopbreak(::evconnlistener_get_base(listener));
}

void Dispatcher::HeartbeatCallback(::evutil_socket_t sock, short what, void* ptr)
{
    Logger::Log(LOG_VERBOSE, "HeartbeatCallback() invoked");
    static_cast<Dispatcher*>(ptr)->PerformHeartbeat();
}

void Dispatcher::StatsCallback(::evutil_socket_t sock, short what, void* ptr)
{
    Logger::Log(LOG_VERBOSE, "StatsCallback() invoked");
    static_cast<Dispatcher*>(ptr)->UpdateStats();
}
