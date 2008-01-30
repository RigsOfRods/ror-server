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
#include "listener.h"
#include <stdexcept>
#include <string>
#include <sstream>

void *s_lsthreadstart(void* vid)
{
	((Listener*)vid)->threadstart();
	return NULL;
}


Listener::Listener(int port)
{
	lport=port;
	//start a listener thread
	pthread_create(&thread, NULL, s_lsthreadstart, this);
}

Listener::~Listener(void)
{
}

void Listener::threadstart()
{
	logmsgf(LOG_DEBUG,"Listerer thread starting");
	//here we start
	SWInetSocket listSocket;
	SWBaseSocket::SWBaseError error;
	
	//manage the listening socket
	listSocket.bind(lport, &error);
	if (error!=SWBaseSocket::ok)
	{
		//this is an error!
		logmsgf(LOG_ERROR,"FATAL Listerer: %s", error.get_error().c_str());
		//there is nothing we can do here
		exit(1);
		return;
	}
	listSocket.listen();
	
	//await connections
	logmsgf(LOG_WARN,"Listener ready");
	while (1)
	{
		logmsgf(LOG_DEBUG,"Listener awaiting connections");
		SWInetSocket *ts=(SWInetSocket *)listSocket.accept(&error);

		logmsgf(LOG_DEBUG,"Listener got a new connection");
		
		if (error!=SWBaseSocket::ok) 
		{
			logmsgf(LOG_ERROR,"ERROR Listener: %s", error.get_error().c_str());
			continue;
		}
		
		//receive a magic
		int type;
		unsigned int source;
		unsigned int len;
		char buffer[256];
		
		try
		{
			// this is the start of it all, it all starts with a simple hello
			if (Messaging::receivemessage(ts, &type, &source, &len, buffer, 256))
				throw std::runtime_error("ERROR Listener: receiving first message");
			
			// make sure our first message is a hello message
			if (type != MSG2_HELLO)
				throw std::runtime_error("ERROR Listener: bad version");
			
			// send client the which version of rornet the server is running
			logmsgf(LOG_DEBUG,"Listener sending version");
			if (Messaging::sendmessage(ts, MSG2_VERSION, 0, strlen(RORNET_VERSION), RORNET_VERSION))
				throw std::runtime_error("ERROR Listener: sending version");
			
			logmsgf(LOG_DEBUG,"Listener sending terrain");
			//send the terrain information back
			if (Messaging::sendmessage( ts, MSG2_TERRAIN_RESP, 0,
					strlen( SEQUENCER.getTerrainName()),
					SEQUENCER.getTerrainName()))
				throw std::runtime_error("ERROR Listener: sending terrain");
	
			if(strncmp(buffer, RORNET_VERSION, strlen(RORNET_VERSION)))
				throw std::runtime_error("ERROR Listener: bad version");
			
			//receive user name
			if (Messaging::receivemessage(ts, &type, &source, &len, buffer, 256))
			{
				std::stringstream error_msg;
				error_msg << "ERROR Listener: receiving user name\n"
					<< "ERROR Listener: got that: "
					<< type;
				throw std::runtime_error(error_msg.str());
			}
			
			if (type != MSG2_USER_CREDENTIALS)
				throw std::runtime_error("Warning Listener: no user name");
			//create a new client
			//correct a bit the client name (trucate, validate)
			if (len>sizeof(user_credentials_t))
				continue;
			logmsgf(LOG_DEBUG,"Listener creating a new client...");
			
			user_credentials_t *user = (user_credentials_t *)buffer;
			if(SEQUENCER.isPasswordProtected())
			{
				logmsgf(LOG_DEBUG,"password login: %s == %s?",
						SEQUENCER.getServerPasswordHash(),
						user->password);
				if(strncmp(SEQUENCER.getServerPasswordHash(), user->password, 40))
				{
					Messaging::sendmessage(ts, MSG2_WRONG_PW, 0, 0, 0);
					throw std::runtime_error( "ERROR Listener: wrong password" );
				}
				
				logmsgf(LOG_DEBUG,"user used the correct password, creating client!");
			}else
			{
				logmsgf(LOG_DEBUG,"creating client, no password protection, creating client");
			}
			SEQUENCER.createClient(ts, user);
			logmsgf(LOG_DEBUG,"listener returned!");
		}
		catch(std::runtime_error e)
		{
			logmsgf(LOG_ERROR, e.what());
			ts->disconnect(&error);
			delete ts;
		}
	}
}
