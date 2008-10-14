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

// RoRserver.cpp : Defines the entry point for the console application.
//
#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"

#include "sha1_util.h"
#include "sha1.h"

#include <iostream>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

#ifdef __WIN32__
#include "windows.h"
#endif

void handler(int signal)
{
	if (signal == SIGINT)
	{
		Logger::log(LOG_ERROR,"got interrupt signal, terminating ...");
	}
	else if (signal == SIGTERM)
	{
		Logger::log(LOG_ERROR,"got termiante signal, terminating ...");
	}
	else
	{
		Logger::log(LOG_ERROR,"got unkown signal: %d", signal);
	}

	if(Config::ServerMode() == SERVER_LAN)
	{
		Logger::log(LOG_ERROR,"closing server ... ");
		Sequencer::cleanUp();
	}
	else
	{
		Logger::log(LOG_ERROR,"closing server ... unregistering ... ");
		Sequencer::unregisterServer();
		Logger::log(LOG_ERROR," unregistered.");
		Sequencer::cleanUp();
	}
	exit(0);
} 

int main(int argc, char* argv[])
{
	// det default verbose levels
	Logger::setLogLevel(LOGTYPE_DISPLAY, LOG_INFO);
	Logger::setLogLevel(LOGTYPE_FILE, LOG_VERBOSE);
	Logger::setFlushLevel(LOG_ERROR);
	Logger::setOutputFile("server.log");


	if( !Config::fromArgs( argc, argv ) ) 
		return 0;
	if( !Config::checkConfig() )
		return 1;
	
	
	if(!sha1check() || sha1_self_test(1))
	{
		Logger::log(LOG_ERROR,"sha1 malfunction!");
		exit(-123);
	}


	// so ready to run, then set up signal handling
	signal(SIGINT, handler);
	signal(SIGTERM, handler);
	
	Sequencer::initilize();

	// start the main program loop
    // if we need to communiate to the master user the notifier routine 
	if(Config::ServerMode() != SERVER_LAN )
	{
		//the main thread is used by the notifier
	    //this should not return untill the server shuts down
		Sequencer::notifyRoutine(); 
	}
	else
	{
		// if not just idle... forever 
		//or by some stupid sleep method in LAN mode
		while (true)
		{
#ifndef __WIN32__
			sleep(60);
#else
			Sleep(60*1000);
#endif
		}
	}

	// delete all (needed in here, if not shutdown due to signal)
	// stick in destructor perhaps? 
	Sequencer::cleanUp();
	return 0;
}
