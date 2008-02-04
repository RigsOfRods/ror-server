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

#include <stdlib.h>
#include <iostream>
#include <csignal>
#include <memory>
#include "sha1_util.h"
#include "sha1.h"

// simpleopt by http://code.jellycan.com/simpleopt/
// license: MIT
#include "SimpleOpt.h"

bool debugmode=false;
int servermode=SERVER_AUTO;


// option identifiers
enum {
	OPT_HELP,
	OPT_IP,
	OPT_PORT,
	OPT_NAME,
	OPT_TERRAIN,
	OPT_MAXCLIENTS,
	OPT_LAN,
	OPT_DEBUG,
	OPT_VERBOSE,
	OPT_VVERBOSE,
	OPT_PASS,
	OPT_INET,
	OPT_RCONPASS,
	OPT_GUI,
	OPT_UPSPEED};

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
	{ OPT_DEBUG, ("-debug"), SO_NONE },
	{ OPT_GUI, ("-gui"), SO_NONE },
	{ OPT_VERBOSE, ("-verbose"), SO_NONE },
	{ OPT_VVERBOSE, ("-vverbose"), SO_NONE },
	{ OPT_HELP,  ("--help"), SO_NONE },
	SO_END_OF_OPTIONS
};


void handler(int signal)
{
	if (signal == SIGINT)
	{
		logmsgf(LOG_ERROR,"got interrupt signal, terminating ...");
	}
	else if (signal == SIGTERM)
	{
		logmsgf(LOG_ERROR,"got termiante signal, terminating ...");
	}
	else
	{
		logmsgf(LOG_ERROR,"got unkown signal: %d", signal);
	}

	if(servermode != SERVER_LAN)
	{
		logmsgf(LOG_ERROR,"closing server ... unregistering ... ");
		if(SEQUENCER.notifier)
			SEQUENCER.notifier->unregisterServer();
		logmsgf(LOG_ERROR," unregistered.");
		SEQUENCER.cleanUp();
	}
	else if(servermode == SERVER_LAN)
	{
		logmsgf(LOG_ERROR,"closing server ... ");
		SEQUENCER.cleanUp();
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
		" -debug\n" \
		"  Print a lot of information\n" \
		" -help\n" \
		"  Show this list\n");
}

void getPublicIP(char *pubip)
{
	char tmp[255];
	SWBaseSocket::SWBaseError error;
	SWInetSocket mySocket;

	if (mySocket.connect(80, REPO_SERVER, &error))
	{
		char query[2048];
		sprintf(query, "GET /getpublicip/ HTTP/1.1\r\nHost: %s\r\n\r\n", REPO_SERVER);
		if(debugmode)
			logmsgf(LOG_DEBUG, "Query to get public IP: %s\n", query);
		if (mySocket.sendmsg(query, &error)<0)
			return;
		int rlen=mySocket.recv(tmp, 250, &error);
		if (rlen>0 && rlen<250) 
			tmp[rlen]=0;
		else
			return;
		if(debugmode)
			logmsgf(LOG_DEBUG, "Response from public IP request :'%s'", tmp);
		//clean a bit
		char *start=strstr(tmp, "\r\n\r\n");
		start+=4;
		//jump chunked size
		start=strstr(start, "\r\n");
		start+=2;
		char* end=strstr(start, "\r\n");

		int len = (int)(end-start);
		strncpy(pubip, start, len);
		//		printf("Response:'%s'\n", pubip);
		// disconnect
		mySocket.disconnect();
	}
	else
		return;
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

	if(!sha1check() || sha1_self_test(1))
	{
		logmsgf(LOG_ERROR,"sha1 malfunction!");
		exit(-123);
	}

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
			} else if (args.OptionId() == OPT_DEBUG) {
				debugmode=true;
				Logger::setLogLevel( LOG_DEBUG );
				logmsgf(LOG_WARN, "== DEBUG MODE ==");
			} else if (args.OptionId() == OPT_VERBOSE) {
				debugmode=true;
				Logger::setLogLevel( LOG_VERBOSE );
				logmsgf(LOG_WARN, "== VERBOSE MODE ==");
			} else if (args.OptionId() == OPT_VVERBOSE) {
				debugmode=true;
				Logger::setLogLevel( LOG_VVERBOSE );
				logmsgf(LOG_WARN, "== VERY VERBOSE MODE ==");
			} else if (args.OptionId() == OPT_LAN) {
				if(servermode != SERVER_AUTO)
				{
					logmsgf(LOG_ERROR, "you cannot use lan mode and internet mode at the same time!");
					exit(1);
				}
				servermode=SERVER_LAN;
				logmsgf(LOG_WARN, "started in LAN mode.");
			} else if (args.OptionId() == OPT_INET) {
				if(servermode != SERVER_AUTO)
				{
					logmsgf(LOG_ERROR, "you cannot use lan mode and internet mode at the same time!");
					exit(1);
				}
				servermode=SERVER_INET;
				logmsgf(LOG_WARN, "started in Internet mode.");
			} else if (args.OptionId() == OPT_MAXCLIENTS) {
				max_clients = atoi(args.OptionArg());
			}
		} else {
			showUsage();
			return 1;
		}
	}

	// some workarounds for missing arguments
	if(strnlen(pubip,250) == 0 && servermode != SERVER_LAN)
	{
		getPublicIP(pubip);
		if(strnlen(pubip,250) == 0)
			logmsgf(LOG_ERROR, "could not get public IP automatically!");
		else
			logmsgf(LOG_ERROR, "got public IP automatically: '%s'", pubip);

	}
	if(listenport == 0)
	{
		listenport = getRandomPort();
		logmsgf(LOG_WARN, "using Port %d", listenport);
	}

	// now validity checks
	if (strnlen(pubip,250) == 0 && servermode != SERVER_LAN)
	{
		logmsgf(LOG_ERROR, "You need to specify a IP where the server will be listening.");
		showUsage();
		return 1;
	}
	if (strnlen(servname,250) == 0 && servermode != SERVER_LAN)
	{
		logmsgf(LOG_ERROR, "You need to specify a server name.");
		showUsage();
		return 1;
	}
	if (strnlen(terrname,250) == 0)
	{
		logmsgf(LOG_ERROR, "You need to specify a terrain name.");
		showUsage();
		return 1;
	}
	if (max_clients < 2 || max_clients > 64)
	{
		logmsgf(LOG_ERROR, "You need to specify how much clients the server should support.");
		showUsage();
		return 1;
	}
	if (listenport == 0)
	{
		logmsgf(LOG_ERROR, "You need to specify a port to listen on.");
		showUsage();
		return 1;
	}
	if(max_clients > 16 && servermode != SERVER_LAN)
	{
		logmsgf(LOG_ERROR, "!!! more than 16 Players not supported in Internet mode.");
		logmsgf(LOG_ERROR, "!!! under full load, you server will use %ikbit/s of upload and %ikbit/s of download", max_clients*(max_clients-1)*64, max_clients*64);
		logmsgf(LOG_ERROR, "!!! that means that clients that have less bandwidth than %ikbit/s of upload and %ikbit/s of download will not be able to join!", 64, (max_clients-1)*64);
	}
	else if(max_clients <= 16)
	{
		logmsgf(LOG_WARN, "app. full load traffic: %ikbit/s upload and %ikbit/s download", max_clients*(max_clients-1)*64, max_clients*64);
	}

	// so ready to run, then set up signal handling
	signal(SIGINT, handler);
	signal(SIGTERM, handler);

	logmsgf(LOG_WARN, "using terrain '%s'. maximum clients: %d", terrname, max_clients);

	if(strnlen(password, 250) > 0)
		logmsgf(LOG_WARN, "server is password protected!");

	if(strnlen(rconpassword, 250) == 0)
		logmsgf(LOG_WARN, "no RCON password set: RCON disabled!");

	SEQUENCER.initilize(pubip, max_clients, servname, terrname, listenport, servermode, password, rconpassword, guimode);

	if(servermode == SERVER_INET || servermode == SERVER_AUTO)
	{
		//the main thread is used by the notifier
		SEQUENCER.notifyRoutine(); //this should not return
	}
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
	SEQUENCER.cleanUp();
	return 0;
}
