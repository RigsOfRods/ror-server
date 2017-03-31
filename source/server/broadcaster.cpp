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

#include <map>

void* StartBroadcasterThread(void* data)
{
    Broadcaster* broadcaster = static_cast<Broadcaster*>(data);
    broadcaster->Thread();
    return nullptr;
}

Broadcaster::Broadcaster(Sequencer* sequencer):
    m_sequencer(sequencer),
    m_client_id(0),
    m_socket(nullptr),
    m_is_running(false),
    m_is_dropping_packets(false)
{
}

void Broadcaster::Start(int client_id, SWInetSocket *socket)
{
    m_client_id = client_id;
    m_socket = socket;
    m_is_running = true;
    m_is_dropping_packets = false;

    m_msg_queue.clear();

    pthread_create(&m_thread, nullptr, StartBroadcasterThread, this);
}

void Broadcaster::Stop()
{
    m_queue_mutex.lock();
    m_is_running = false;
    m_queue_cond.signal();
    m_queue_mutex.unlock();

    pthread_join(m_thread, nullptr);
}

void Broadcaster::Thread()
{
    Logger::Log(LOG_DEBUG, "broadcaster m_thread %u owned by client_id %d", ThreadID::getID(), m_client_id);
    while( m_is_running )
    {
        queue_entry_t msg;
        // define a new scope and use a scope lock
        {
            MutexLocker scoped_lock( m_queue_mutex );
            while( m_msg_queue.empty() && m_is_running)
            {
                m_queue_mutex.wait( m_queue_cond );
            }
            if (!m_is_running)
            {
                break;
            }

            msg = m_msg_queue.front();
            m_msg_queue.pop_front();
        }

        if (msg.is_dropping)
        {
            Messaging::StatsAddOutgoingDrop(sizeof(header_t) + msg.datalen); // Statistics
        }
        else
        {
            // TODO WARNING THE SOCKET IS NOT PROTECTED!!!
            if (Messaging::SendMessage(m_socket, msg.type, msg.uid, msg.streamid, msg.datalen, msg.data) != 0)
            {
                m_sequencer->disconnect(m_client_id, "Broadcaster: Send error", true, true);
                break;
            }
        }
    }

    if (m_is_running)
    {
        MutexLocker scoped_lock(m_queue_mutex);
        m_is_running = false;
        m_queue_mutex.wait(m_queue_cond);
    }
}

//this is called all the way from the receiver threads, we should process this swiftly
//and keep in mind that it is called crazily and concurently from lots of threads
//we MUST copy the data too
//also, this function can be called by threads owning clients_mutex !!!
void Broadcaster::QueueMessage(int type, int uid, unsigned int streamid, unsigned int len, const char* data)
{
    if (!m_is_running)
    {
        return;
    }
    // for now lets just queue msgs in the order received to make things simple
    queue_entry_t msg = { false, type, uid, streamid, "", len};
    memset( msg.data, 0, MAX_MESSAGE_LENGTH );
    memcpy( msg.data, data, len );

    MutexLocker scoped_lock( m_queue_mutex );

    // we will limit the entries in this queue
    
    // soft limit: we start dropping data packages
    if((m_msg_queue.size() > (size_t)QUEUE_SOFT_LIMIT) && (type == MSG2_STREAM_DATA))
    {
        Logger::Log(LOG_DEBUG, "broadcaster queue soft full: m_thread %u owned by uid %d", ThreadID::getID(), m_client_id);
        msg.is_dropping = true;
        m_is_dropping_packets = true;
    }
    else if (m_msg_queue.size() < (size_t)QUEUE_SOFT_LIMIT - 20) // - 20 to prevent border problems
    {
        m_is_dropping_packets = false;
    }

    // hard limit drop anything, otherwise we would need to run through the queue and search and remove
    // data packages, which is not really feasible
    if(m_msg_queue.size() > (size_t)QUEUE_HARD_LIMIT)
    {
        Logger::Log(LOG_DEBUG, "broadcaster queue hard full: m_thread %u owned by uid %d", ThreadID::getID(), m_client_id);
        msg.is_dropping = true;
    }
    
    if (msg.is_dropping)
    {
        Messaging::StatsAddOutgoingDrop(sizeof(header_t) + msg.datalen); // Statistics
    }
    else
    {
        m_msg_queue.push_back( msg );
        //signal the thread that new data is waiting to be sent
        m_queue_cond.signal();
    }
}


