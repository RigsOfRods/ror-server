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
	unsigned int len;
	SWBaseSocket::SWBaseError error;
	
	if(Sequencer::isbanned(sock->get_peerAddr(&error).c_str()))
	{
		Logger::log( LOG_DEBUG, "receiver thread %d owned by uid %d terminated (banned user)", ThreadID::getID(), id);
		Logger::log(LOG_VERBOSE,"banned user rejected: uid %i", id);
		Messaging::sendmessage(sock, MSG2_BANNED, id, 0, 0);
		Sequencer::disconnect(id, "you are banned");
		return;
	}

	Logger::log(LOG_VERBOSE,"Sending welcome message to uid %i", id);
	if( Messaging::sendmessage(sock, MSG2_WELCOME, id, 0, 0) )
	{
		Sequencer::disconnect( id, "error sending welcome message" );
		return;
	}
	
	//security fix: we limit the size of the vehicle name to 128 characters <- from Luigi Auriemma
	if (Messaging::receivemessage(sock, &type, &source, &len, dbuffer, 128)) {
		Sequencer::disconnect(id, "Messaging abuse 1");
		return;
	}
	
	if (type!=MSG2_USE_VEHICLE)
	{
		Sequencer::disconnect(id, "Protocol error 1");
		return;
	}
	//security
	dbuffer[len]=0;
	//we queue the use vehicle message for others
	Sequencer::queueMessage(id, type, dbuffer, len);
	
	//get the buffer size, not really usefull but a good way to detect errors
	if (Messaging::receivemessage(sock, &type, &source, &len, dbuffer, 4))
	{
		Sequencer::disconnect(id, "Messaging abuse 2"); 
		return;
	}
	
	if (type!=MSG2_BUFFER_SIZE)
	{
		Sequencer::disconnect(id, "Protocol error 2");
		return;
	}
	
	unsigned int buffersize=*((unsigned int*)dbuffer);
	if (buffersize>MAX_MESSAGE_LENGTH)
	{
		Sequencer::disconnect(id, "Memory error from client");
		return;
	}
	//notify the client of all pre-existing vehicles
	Sequencer::notifyAllVehicles(id);
	
	//okay, we are ready, we can receive data frames
	Sequencer::enableFlow(id);

	//send motd
	Sequencer::sendMOTD(id);
	
	Logger::log(LOG_VERBOSE,"UID %d is switching to FLOW", id);
	
	// this prevents the socket from hangingwhen sending data
	// which is the cause of threads getting blocked
	sock->set_timeout(60, 0);
	while( running )
	{
		if (Messaging::receivemessage(sock, &type, &source, &len, dbuffer, MAX_MESSAGE_LENGTH))
		{
			Sequencer::disconnect(id, "Game connection closed");
			break;
		}
		if( !running ) break;
		
		if (type!=MSG2_VEHICLE_DATA && 
				type!=MSG2_CHAT &&
				type!=MSG2_FORCE &&
				type!=MSG2_PRIVCHAT &&
				type!=MSG2_VEHICLE_BEAMS &&
				type!=MSG2_REQUEST_VEHICLE_BEAMS &&
				type!=MSG2_DELETE)
		{
			Sequencer::disconnect(id, "Protocol error 3");
			break;
		}
		Sequencer::queueMessage(id, type, dbuffer, len);
	}
}
