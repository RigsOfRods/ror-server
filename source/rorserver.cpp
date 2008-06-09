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
#include "SocketW.h"
#include "rornet.h"
#include "sequencer.h"
#include "HttpMsg.h"
#include "logger.h"

#include "sha1_util.h"
#include "sha1.h"

#include <stdlib.h>
#include <iostream>
#include <csignal>
#include <stdexcept>



// simpleopt by http://code.jellycan.com/simpleopt/
// license: MIT
#include "SimpleOpt.h"

int servermode=SERVER_AUTO;


// option identifiers
enum
{
	OPT_HELP,
	OPT_IP,
	OPT_PORT,
	OPT_NAME,
	OPT_TERRAIN,
	OPT_MAXCLIENTS,
	OPT_LAN,
	OPT_VERBOSITY,
	OPT_LOGVERBOSITY,
	OPT_PASS,
	OPT_INET,
	OPT_RCONPASS,
	OPT_GUI,
	OPT_UPSPEED,
	OPT_LOGFILENAME
};

// option array
CSimpleOpt::SOption cmdline_options[] = {
	{ OPT_IP,   ("-ip"),   SO_REQ_SEP },
	{ OPT_PORT, ("-port"), SO_REQ_SEP },
	{ OPT_NAME, ("-name"), SO_REQ_SEP },
	{ OPT_PASS, ("-password"), SO_REQ_SEP },
	{ OPT_RCONPASS, ("-rconpassword"), SO_REQ_SEP },
	{ OPT_TERRAIN, ("-terrain"), SO_REQ_SEP },
	{ OPT_MAXCLIENTS, ("-maxclients"), SO_REQ_SEP },
	{ OPT_UPSPEED, ("-speed"), SO_REQ_SEP },
	{ OPT_LAN, ("-lan"), SO_NONE },
	{ OPT_INET, ("-inet"), SO_NONE },
	{ OPT_VERBOSITY, ("-verbosity"), SO_REQ_SEP },
	{ OPT_LOGVERBOSITY, ("-logverbosity"), SO_REQ_SEP },
	{ OPT_LOGFILENAME, ("-logfilename"), SO_REQ_SEP },
	{ OPT_GUI, ("-gui"), SO_NONE },
	{ OPT_HELP,  ("--help"), SO_NONE },
	SO_END_OF_OPTIONS
};


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

	if(servermode != SERVER_LAN)
	{
		Logger::log(LOG_ERROR,"closing server ... unregistering ... ");
		Sequencer::unregisterServer();
		Logger::log(LOG_ERROR," unregistered.");
		Sequencer::cleanUp();
	}
	else if(servermode == SERVER_LAN)
	{
		Logger::log(LOG_ERROR,"closing server ... ");
		Sequencer::cleanUp();
	}
	exit(0);
} 

void showUsage()
{
	printf("\n" \
		"Usage: rorserver [OPTIONS] <paramaters>\n" \
		" Where [OPTIONS] and <parameters>\n" \
		" REQUIRED:\n" \
		" -name <name>\n" \
		"  Name of the server, no spaces, only [a-z,0-9,A-Z]\n" \
		" -terrain <mapname>\n" \
		"  Map name (defaults to 'any')\n" \
		" -maxclients|speed <clients|speed>\n" \
		"  Maximum clients allowed or maximum upload speed (in kilobits)\n" \
		" -lan|inet \n" \
		"  Private or public server (defaults to inet)\n" \
		" OPTIONAL:\n" \
		" -gui (EXPERIMENTAL!)\n" \
		"  Use organizational terminal GUI\n" \
		" -password <password>\n" \
		"  Private server password\n" \
		" -rconpassword <password>\n" \
		"  Set admin password. This is required for RCON to work. Otherwise RCON is disabled.\n" \
		" -ip <ip>\n" \
		"  Public IP address to register with.\n" \
		" -port <port>\n" \
		"  Port to use (defaults to random 12000-12500)\n" \
		" -verbosity {0-5}\n" \
		"  Sets displayed log verbosity: 0 = stack, 1 = debug, 2 = verbosity, 3 = info, 4 = warn, 5 = error\n" \
		" -logverbosity {0-5}\n" \
		"  Sets file log verbosity: 0 = stack, 1 = debug, 2 = verbosity, 3 = info, 4 = warn, 5 = error\n" \
		" -logfilename <server.log>" \
		"  sets the filename of the log" \
		" -help\n" \
		"  Show this list\n");
}

void getPublicIP(char *pubip)
{
	char tmp[255];
	SWBaseSocket::SWBaseError error;
	SWInetSocket mySocket;

	try
	{
	if (mySocket.connect(80, REPO_SERVER, &error))
	{
		char query[2048];
		sprintf(query, "GET /getpublicip/ HTTP/1.1\r\nHost: %s\r\n\r\n", REPO_SERVER);
		Logger::log(LOG_DEBUG, "Query to get public IP: %s\n", query);
		if (mySocket.sendmsg(query, &error)<0)
			return;
		int rlen=mySocket.recv(tmp, 250, &error);
		if (rlen>0 && rlen<250) 
			tmp[rlen]=0;
		else
			return;
		Logger::log(LOG_DEBUG, "Response from public IP request :'%s'", tmp);
		
		HttpMsg msg(tmp);
		strncpy(pubip, msg.getBody().c_str(), msg.getBody().length());
		//		printf("Response:'%s'\n", pubip);
		
		// disconnect
		mySocket.disconnect();
	}
	
	}
	catch( std::runtime_error e )
	{
		Logger::log( LOG_ERROR, e.what() );
	}
}

int getRandomPort()
{
	srand ((int)time (0));
	return 12000 + (rand()%500);
}


int main(int argc, char* argv[])
{

	char pubip[255]="";
	int listenport=0;
	char servname[255]="";
	char terrname[255]="any";
	char password[255]="";
	char rconpassword[255]="";
	int max_clients=16;
	bool guimode=false;

	// parse arguments
	CSimpleOpt args(argc, argv, cmdline_options);
	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			if (args.OptionId() == OPT_HELP) {
				showUsage();
				return 0;
			} else if (args.OptionId() == OPT_IP) {
				strncpy(pubip, args.OptionArg(), 250);
			} else if (args.OptionId() == OPT_NAME) {
				strncpy(servname, args.OptionArg(), 250);
			} else if (args.OptionId() == OPT_LOGFILENAME) {
				Logger::setOutputFile(std::string(args.OptionArg()));
			} else if (args.OptionId() == OPT_TERRAIN) {
				strncpy(terrname, args.OptionArg(), 250);
			} else if (args.OptionId() == OPT_PASS) {
				strncpy(password, args.OptionArg(), 250);
			} else if (args.OptionId() == OPT_UPSPEED) {
				max_clients = (int)floor((1+sqrt((float)1-4*(-(atoi(args.OptionArg())/64))))/2);
			} else if (args.OptionId() == OPT_RCONPASS) {
				strncpy(rconpassword, args.OptionArg(), 250);
			} else if (args.OptionId() == OPT_PORT) {
				listenport = atoi(args.OptionArg());
			} else if (args.OptionId() == OPT_GUI) {
				guimode=true;
			} else if (args.OptionId() == OPT_VERBOSITY) {
				Logger::setLogLevel(LOGTYPE_DISPLAY, LogLevel(atoi(args.OptionArg())));
			} else if (args.OptionId() == OPT_LOGVERBOSITY) {
				Logger::setLogLevel(LOGTYPE_FILE, LogLevel(atoi(args.OptionArg())));
			} else if (args.OptionId() == OPT_LAN) {
				if(servermode != SERVER_AUTO)
				{
					Logger::log(LOG_ERROR, "you cannot use lan mode and internet mode at the same time!");
					exit(1);
				}
				servermode=SERVER_LAN;
				Logger::log(LOG_WARN, "started in LAN mode.");
			} else if (args.OptionId() == OPT_INET) {
				if(servermode != SERVER_AUTO)
				{
					Logger::log(LOG_ERROR, "you cannot use lan mode and internet mode at the same time!");
					exit(1);
				}
				servermode=SERVER_INET;
			} else if (args.OptionId() == OPT_MAXCLIENTS) {
				max_clients = atoi(args.OptionArg());
			}
		} else {
			showUsage();
			return 1;
		}
	}

	if(!sha1check() || sha1_self_test(1))
	{
		Logger::log(LOG_ERROR,"sha1 malfunction!");
		exit(-123);
	}

	if(servermode==SERVER_AUTO)
		Logger::log(LOG_INFO, "server started in automatic mode.");
	else if(servermode==SERVER_LAN)
		Logger::log(LOG_INFO, "server started in LAN mode.");
	else if(servermode==SERVER_INET)
		Logger::log(LOG_INFO, "server started in Internet mode.");

	// some workarounds for missing arguments
	if(strnlen(pubip,250) == 0 && servermode != SERVER_LAN)
	{
		getPublicIP(pubip);
		if(strnlen(pubip,250) == 0)
			Logger::log(LOG_ERROR, "could not get public IP automatically!");
		else
			Logger::log(LOG_INFO, "got public IP automatically: '%s'", pubip);

	}
	if(listenport == 0)
	{
		listenport = getRandomPort();
		Logger::log(LOG_INFO, "using Port %d", listenport);
	}

	// now validity checks
	if (strnlen(pubip,250) == 0 && servermode != SERVER_LAN)
	{
		Logger::log(LOG_ERROR, "You need to specify a IP where the server will be listening.");
		showUsage();
		return 1;
	}
	if (strnlen(servname,250) == 0 && servermode != SERVER_LAN)
	{
		Logger::log(LOG_ERROR, "You need to specify a server name.");
		showUsage();
		return 1;
	}
	if (strnlen(terrname,250) == 0)
	{
		Logger::log(LOG_ERROR, "You need to specify a terrain name.");
		showUsage();
		return 1;
	}
	if (max_clients < 2 || max_clients > 64)
	{
		Logger::log(LOG_ERROR, "You need to specify how much clients the server should support.");
		showUsage();
		return 1;
	}
	if (listenport == 0)
	{
		Logger::log(LOG_ERROR, "You need to specify a port to listen on.");
		showUsage();
		return 1;
	}
	if(max_clients > 16 && servermode != SERVER_LAN)
	{
		Logger::log(LOG_ERROR, "!!! more than 16 Players not supported in Internet mode.");
		Logger::log(LOG_ERROR, "!!! under full load, you server will use %ikbit/s of upload and %ikbit/s of download", max_clients*(max_clients-1)*64, max_clients*64);
		Logger::log(LOG_ERROR, "!!! that means that clients that have less bandwidth than %ikbit/s of upload and %ikbit/s of download will not be able to join!", 64, (max_clients-1)*64);
	}
	else if(max_clients <= 16)
	{
		Logger::log(LOG_WARN, "app. full load traffic: %ikbit/s upload and %ikbit/s download", max_clients*(max_clients-1)*64, max_clients*64);
	}

	// so ready to run, then set up signal handling
	signal(SIGINT, handler);
	signal(SIGTERM, handler);

	Logger::log(LOG_WARN, "using terrain '%s'. maximum clients: %d", terrname, max_clients);

	if(strnlen(password, 250) > 0)
		Logger::log(LOG_WARN, "server is password protected!");

	if(strnlen(rconpassword, 250) == 0)
		Logger::log(LOG_WARN, "no RCON password set: RCON disabled!");

	Sequencer::initilize(pubip, max_clients, servname, terrname, listenport, servermode, password, rconpassword, guimode);

	// start the main program loop
    // if we need to communiate to the master user the notifier routine 
	if(servermode == SERVER_INET || servermode == SERVER_AUTO)
	{
		//the main thread is used by the notifier
	    //this should not return untill the server shuts down
		Sequencer::notifyRoutine(); 
	}
	// if not just idle... forever 
	else if(servermode == SERVER_LAN)
	{
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
