/*
This file is part of "Rigs of Rods Server" (Relay mode)
Copyright 2007 Pierre-Michel Ricordel
Contact: pricorde@rigsofrods.com
"Rigs of Rods Server" is distributed under the terms of the GNU General Public License.

"Rigs of Rods Server" is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.

"Rigs of Rods Server" is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "receiver.h"
#include "SocketW.h"
#include "sequencer.h"
#include "messaging.h"
#include "ScriptEngine.h"
#include "logger.h"

void *s_lithreadstart(void* vid)
{
    STACKLOG;
	((Receiver*)vid)->threadstart();
#ifdef WIN32	
	Logger::log( LOG_DEBUG, "Receiver thread %u:%u is exiting",
	        (unsigned int) pthread_self().p, ThreadID::getID() );
#endif
	return NULL;
}


Receiver::Receiver()
: id(0), sock(NULL), running( false )
{
    STACKLOG;
}

Receiver::~Receiver(void)
{
    STACKLOG;
	stop();
}

void Receiver::reset(int pos, SWInetSocket *socky)
{
    STACKLOG;
	id    = pos;
	sock  = socky;
	running = true;
	
	//start a listener thread
	pthread_create(&thread, NULL, s_lithreadstart, this);
}

void Receiver::stop()
{
    STACKLOG;
	if(!running) return; // already called, discard call
    running = false;
#ifdef WIN32
    Logger::log( LOG_DEBUG, "joining with receiver thread: %u",
            (unsigned int) &thread.p);
#endif
	pthread_join(thread, NULL);
}

void Receiver::threadstart()
{
    STACKLOG;
	Logger::log( LOG_DEBUG, "receiver thread %d owned by uid %d", ThreadID::getID(), id);
	//get the vehicle description
	int type;
	int source;
	unsigned int streamid;
	unsigned int len;
	SWBaseSocket::SWBaseError error;
	//okay, we are ready, we can receive data frames
	Sequencer::enableFlow(id);

	//send motd
	Sequencer::sendMOTD(id);

#ifdef WITH_ANGELSCRIPT
	if(Sequencer::getScriptEngine())
		Sequencer::getScriptEngine()->playerAdded(id);
#endif //WITH_ANGELSCRIPT

	Logger::log(LOG_VERBOSE,"UID %d is switching to FLOW", id);
	
	// this prevents the socket from hangingwhen sending data
	// which is the cause of threads getting blocked
	sock->set_timeout(60, 0);
	while( running )
	{
		if (Messaging::receivemessage(sock, &type, &source, &streamid, &len, dbuffer, MAX_MESSAGE_LENGTH))
		{
			Sequencer::disconnect(id, "Game connection closed");
			break;
		}
		if( !running ) break;
		
		if(type != MSG2_STREAM_DATA)
			Logger::log(LOG_VERBOSE,"got message: type: %d, source: %d:%d, len: %d", type, source, streamid, len);
		
		if (type < 1000 || type > 1050)
		{
			Sequencer::disconnect(id, "Protocol error 3");
			break;
		}
		Sequencer::queueMessage(id, type, streamid, dbuffer, len);
	}
}
