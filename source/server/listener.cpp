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

#ifdef __GNUC__
#include <stdlib.h>
#endif

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
	//pthread_join( thread, NULL );
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
		unsigned int streamid;
		char buffer[256];

		try
		{
			// this is the start of it all, it all starts with a simple hello
			if (Messaging::receivemessage(ts, &type, &source, &streamid, &len, buffer, 256))
				throw std::runtime_error("ERROR Listener: receiving first message");

			// make sure our first message is a hello message
			if (type != MSG2_HELLO)
			{
				Messaging::sendmessage(ts, MSG2_WRONG_VER, 0, 0, 0, 0);
				throw std::runtime_error("ERROR Listener: protocol error");
			}

			// check client version
			if(source == 5000 && (std::string(buffer) == "MasterServ"))
			{
				Logger::log(LOG_VERBOSE, "Master Server knocked ...");
				// send back some information, then close socket
				char tmp[2048]="";
				sprintf(tmp,"protocol:%s\nrev:%s\nbuild_on:%s_%s\n", RORNET_VERSION, VERSION,__DATE__, __TIME__);
				if (Messaging::sendmessage(ts, MSG2_MASTERINFO, 0, 0, (unsigned int) strlen(tmp), tmp))
				{
					throw std::runtime_error("ERROR Listener: sending master info");
				}
				// close socket
				ts->disconnect(&error);
				delete ts;
			}

			if(strncmp(buffer, RORNET_VERSION, strlen(RORNET_VERSION)))
			{
				Messaging::sendmessage(ts, MSG2_WRONG_VER, 0, 0, 0, 0);
				throw std::runtime_error("ERROR Listener: bad version: "+std::string(buffer)+". rejecting ...");
			}

			// send client the which version of rornet the server is running
			Logger::log(LOG_DEBUG,"Listener sending version");
			if (Messaging::sendmessage(ts, MSG2_VERSION, 0, 0,
					(unsigned int) strlen(RORNET_VERSION), RORNET_VERSION))
				throw std::runtime_error("ERROR Listener: sending version");

			Logger::log(LOG_DEBUG,"Listener sending terrain");
			//send the terrain information back
			if( Messaging::sendmessage( ts, MSG2_TERRAIN_RESP, 0, 0,
					(unsigned int) Config::getTerrainName().length(),
					Config::getTerrainName().c_str() ) )
				throw std::runtime_error("ERROR Listener: sending terrain");

			//receive user name
			if (Messaging::receivemessage(ts, &type, &source, &streamid, &len, buffer, 256))
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
			int authflags = Sequencer::authNick(std::string(user->uniqueid), nickname);
			if(authflags & AUTH_RANKED)
			{
				// we only auth here in order to overwrite the nickname!
				Logger::log(LOG_INFO, "User %s is ranked", nickname.c_str());
				strncpy(user->username, nickname.c_str(), 20);
			} else
				Logger::log(LOG_INFO, "User %s is NOT ranked", nickname.c_str());

			if( Config::isPublic() )
			{
				Logger::log(LOG_DEBUG,"password login: %s == %s?",
						Config::getPublicPassword().c_str(),
						user->password);
				if(strncmp(Config::getPublicPassword().c_str(), user->password, 40))
				{
					Messaging::sendmessage(ts, MSG2_WRONG_PW, 0, 0, 0, 0);
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
