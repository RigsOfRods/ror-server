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
#include "rornet.h"
#include "messaging.h"
#include "sequencer.h"
#include "SocketW.h"
#include "logger.h"
#include "config.h"

#include <stdexcept>
#include <string>
#include <sstream>

void *s_lsthreadstart(void* vid)
{
    STACKLOG;
	((Listener*)vid)->threadstart();
	return NULL;
}


Listener::Listener(int port): lport( port )
{
    STACKLOG;
	//start a listener thread
	pthread_create(&thread, NULL, s_lsthreadstart, this);
}

Listener::~Listener(void)
{
    STACKLOG;
}

void Listener::threadstart()
{
    STACKLOG;
	Logger::log(LOG_DEBUG,"Listerer thread starting");
	//here we start
	SWInetSocket listSocket;
	SWBaseSocket::SWBaseError error;
	
	//manage the listening socket
	listSocket.bind(lport, &error);
	if (error!=SWBaseSocket::ok)
	{
		//this is an error!
		Logger::log(LOG_ERROR,"FATAL Listerer: %s", error.get_error().c_str());
		//there is nothing we can do here
		exit(1);
	}
	listSocket.listen();
	
	//await connections
	Logger::log(LOG_VERBOSE,"Listener ready");
	while (1)
	{
		Logger::log(LOG_VERBOSE,"Listener awaiting connections");
		SWInetSocket *ts=(SWInetSocket *)listSocket.accept(&error);
		
		if (error!=SWBaseSocket::ok) 
		{
			Logger::log(LOG_ERROR,"ERROR Listener: %s", error.get_error().c_str());
			continue;
		}

		
		Logger::log(LOG_VERBOSE,"Listener got a new connection");
#ifndef NOTIMEOUT
		ts->set_timeout(600, 0);
#endif
		
		//receive a magic
		int type;
		int source;
		unsigned int len;
		char buffer[256];
		
		try
		{
			// this is the start of it all, it all starts with a simple hello
			if (Messaging::receivemessage(ts, &type, &source, &len, buffer, 256))
				throw std::runtime_error("ERROR Listener: receiving first message");
			
			// make sure our first message is a hello message
			if (type != MSG2_HELLO)
				throw std::runtime_error("ERROR Listener: protocol error");
			
			// send client the which version of rornet the server is running
			Logger::log(LOG_DEBUG,"Listener sending version");
			if (Messaging::sendmessage(ts, MSG2_VERSION, 0,
					(unsigned int) strlen(RORNET_VERSION), RORNET_VERSION))
				throw std::runtime_error("ERROR Listener: sending version");

			Logger::log(LOG_DEBUG,"Listener sending terrain");
			//send the terrain information back
			if( Messaging::sendmessage( ts, MSG2_TERRAIN_RESP, 0,
					(unsigned int) Config::TerrainName().length(),
					Config::TerrainName().c_str() ) )
				throw std::runtime_error("ERROR Listener: sending terrain");
	
			// original code was source  = 5000 should it be left like this
			// or was the intention source == 5000?
			if( source == 5000 && (std::string(buffer) == "MasterServ") )
			{
				Logger::log(LOG_VERBOSE, "Master Server knocked ...");
				continue;
			}

			if(strncmp(buffer, RORNET_VERSION, strlen(RORNET_VERSION)) && source != 5000) // source 5000 = master server (harcoded)
				throw std::runtime_error("ERROR Listener: bad version: "+std::string(buffer));
			
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
			
			if (len > sizeof(user_credentials_t))
				throw std::runtime_error( "Error: did not receive proper user "
						"credentials" );
			Logger::log(LOG_INFO,"Listener creating a new client...");
			
			user_credentials_t *user = (user_credentials_t *)buffer;
			std::string nickname = std::string(user->username);
			int res = Sequencer::authNick(std::string(user->uniqueid), nickname);
			if(!res)
			{
				Logger::log(LOG_INFO, "User %s is authed", nickname.c_str());
				strncpy(user->username, nickname.c_str(), 20);
			}

			if( Config::isPublic() )
			{
				Logger::log(LOG_DEBUG,"password login: %s == %s?",
						Config::PublicPassword().c_str(),
						user->password);
				if(strncmp(Config::PublicPassword().c_str(), user->password, 40))
				{
					Messaging::sendmessage(ts, MSG2_WRONG_PW, 0, 0, 0);
					throw std::runtime_error( "ERROR Listener: wrong password" );
				}
				
				Logger::log(LOG_DEBUG,"user used the correct password, "
						"creating client!");
			} else {
				Logger::log(LOG_DEBUG,"no password protection, creating client");
			}

			//create a new client
			Sequencer::createClient(ts, user);
			Logger::log(LOG_DEBUG,"listener returned!");
		}
		catch(std::runtime_error e)
		{
			Logger::log(LOG_ERROR, e.what());
			ts->disconnect(&error);
			delete ts;
		}
	}
}
