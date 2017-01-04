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

void* LaunchReceiverThread(void* data)
{
    Receiver* receiver = static_cast<Receiver*>(data);
    receiver->Thread();
    return nullptr;
}

Receiver::Receiver(Sequencer* sequencer):
    m_id(0),
    m_socket(nullptr),
    m_is_running(false),
    m_sequencer(sequencer)
{
}

Receiver::~Receiver(void)
{
    Stop();
}

void Receiver::Start(int pos, SWInetSocket *socky)
{
    m_id = pos;
    m_socket = socky;
    m_is_running = true;

    pthread_create(&m_thread, nullptr, LaunchReceiverThread, this);
}

void Receiver::Stop()
{
    if (!m_is_running)
    {
        return;
    }
    m_is_running = false;

    pthread_join(m_thread, nullptr);
}

void Receiver::Thread()
{
    Logger::Log( LOG_DEBUG, "receiver thread %d owned by uid %d", ThreadID::getID(), m_id);
    //get the vehicle description
    int type;
    int source;
    unsigned int streamid;
    unsigned int len;
    SWBaseSocket::SWBaseError error;
    //okay, we are ready, we can receive data frames
    m_sequencer->enableFlow(m_id);

    //send motd
    m_sequencer->sendMOTD(m_id);

    Logger::Log(LOG_VERBOSE,"UID %d is switching to FLOW", m_id);
    
    // this prevents the socket from hangingwhen sending data
    // which is the cause of threads getting blocked
    m_socket->set_timeout(60, 0);
    while( m_is_running )
    {
        if (Messaging::ReceiveMessage(m_socket, &type, &source, &streamid, &len, m_dbuffer, RORNET_MAX_MESSAGE_LENGTH))
        {
            m_sequencer->disconnect(m_id, "Game connection closed");
            break;
        }

        if (!m_is_running)
        {
            break;
        }
        
        if (type != RoRnet::MSG2_STREAM_DATA)
        {
            Logger::Log(LOG_VERBOSE, "got message: type: %d, source: %d:%d, len: %d", type, source, streamid, len);
        }
        
        if (type < 1000 || type > 1050)
        {
            m_sequencer->disconnect(m_id, "Protocol error 3");
            break;
        }
        m_sequencer->queueMessage(m_id, type, streamid, m_dbuffer, len);
    }
}
