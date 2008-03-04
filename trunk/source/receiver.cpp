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

void *s_lithreadstart(void* vid)
{
    STACKLOG;
	((Receiver*)vid)->threadstart();
	return NULL;
}


Receiver::Receiver()
{
    STACKLOG;
	id=0;
	sock=0;
	alive=false;
}

Receiver::~Receiver(void)
{
    STACKLOG;
	stop();
}

void Receiver::reset(int pos, SWInetSocket *socky)
{
    STACKLOG;
	if (alive)
	{
		Logger::log(LOG_ERROR,"Whoa, receiver is still alive!");
		return;
	}
	id=pos;
	sock=socky;
	alive=true;
	//start a listener thread
	pthread_create(&thread, NULL, s_lithreadstart, this);
}

void Receiver::stop()
{
    STACKLOG;
	//pthread_cancel(thread);
	//pthread_join(thread, NULL);
	alive=false;
}

void Receiver::threadstart()
{
    STACKLOG;
	Logger::log(LOG_DEBUG,"Receive thread %d started", id);
	//get the vehicle description
	int type;
	unsigned int source;
	unsigned int len;
	
	//security fix: we limit the size of the vehicle name to 128 characters <- from Luigi Auriemma
	if (Messaging::receivemessage(sock, &type, &source, &len, dbuffer, 128)) {
		SEQUENCER.disconnect(id, "Messaging abuse 1");
		pthread_exit(NULL);
	}
	
	if (type!=MSG2_USE_VEHICLE) {
		SEQUENCER.disconnect(id, "Protocol error 1");
		pthread_exit(NULL);
	}
	//security
	dbuffer[len]=0;
	//we queue the use vehicle message for others
	SEQUENCER.queueMessage(id, type, dbuffer, len);
	//get the buffer size, not really usefull but a good way to detect errors
	if (Messaging::receivemessage(sock, &type, &source, &len, dbuffer, 4)) {
		SEQUENCER.disconnect(id, "Messaging abuse 2"); 
		pthread_exit(NULL);
	}
	
	if (type!=MSG2_BUFFER_SIZE) {
		SEQUENCER.disconnect(id, "Protocol error 2");
		pthread_exit(NULL);
	}
	unsigned int buffersize=*((unsigned int*)dbuffer);
	if (buffersize>MAX_MESSAGE_LENGTH) {
		SEQUENCER.disconnect(id, "Memory error from client");
		pthread_exit(NULL);
	}
	//notify the client of all pre-existing vehicles
	SEQUENCER.notifyAllVehicles(id);
	//okay, we are ready, we can receive data frames
	SEQUENCER.enableFlow(id);
	Logger::log(LOG_VERBOSE,"UID %d is switching to FLOW", id);
	while (1)
	{
		//	hmm for some reason this fails, 
		if (Messaging::receivemessage(sock, &type, &source, &len, dbuffer, MAX_MESSAGE_LENGTH)) {
			SEQUENCER.disconnect(id, "Game connection closed");
			pthread_exit(NULL);
		}
		//shouldn't we already have truck data at this point?? 
		if (type!=MSG2_VEHICLE_DATA && 
				type!=MSG2_CHAT &&
				type!=MSG2_FORCE &&
				type!=MSG2_RCON_LOGIN &&
				type!=MSG2_RCON_COMMAND &&
				type!=MSG2_DELETE) {
			SEQUENCER.disconnect(id, "Protocol error 3");
			pthread_exit(NULL);
		}
		SEQUENCER.queueMessage(id, type, dbuffer, len);
	}
	pthread_exit(NULL);
}
