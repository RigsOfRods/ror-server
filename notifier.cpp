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
#include "SocketW.h"
#include "notifier.h"

#ifndef __WIN32__
#include <unistd.h>
#endif

Notifier::Notifier(char* pubip, int port, int max_client, char* servname, char* terrname, bool pprotected, int smode) : exit(false)
{
	lport=port;
	maxclient=max_client;
	server_name=servname;
	terrain_name=terrname;
	public_ip=pubip;
	passprotected=pprotected;
	wasregistered=false;
	servermode=smode;
	error_count=0;
}

Notifier::~Notifier(void)
{
	exit = true;
}

/**
 * @brief
 */
bool Notifier::registerServer()
{
	char regurl[1024];
	sprintf(regurl, "%s/register/?name=%s&description=%s&ip=%s&port=%i&terrainname=%s&maxclients=%i&version=%s&pw=%d", 
		REPO_URLPREFIX, server_name, "", public_ip, lport, terrain_name, maxclient, RORNET_VERSION, passprotected);
	printf("Trying to register at Master Server ... (this can take some seconds as your server is being checked by the Master server)\n");
	if (HTTPGET(regurl) < 0)
		return false;

	if(strncmp(httpresp+5, "error", 5) == 0) {
		// print out the error.
		printf("%s\n\n", httpresp+5);
		printf("!!! Server is NOT registered at the Master server !!!\n");
		wasregistered=false;
		return false;
	}
	else
	{
		strncpy(challenge, httpresp+4, 40);
		challenge[40]=0;
		//	printf("Challenge:%s\n", challenge);
		printf("Server is registered at the Master server.\n");
		wasregistered=true;
		return true;
	}
}

bool Notifier::unregisterServer()
{
	if(!wasregistered)
		return false;
	char unregurl[1024];
	sprintf(unregurl, "%s/unregister/?challenge=%s", REPO_URLPREFIX, challenge);
	if (HTTPGET(unregurl) < 0)
		return false;

	if(strncmp(httpresp, "2\r\nok", 5) >= 0)
		return true;
	else
		return false;
}

bool Notifier::sendHearbeat()
{
	char hearbeaturl[1024] = "";
	char hearbeatdata[16384] = "";
	sprintf(hearbeaturl, "%s/heartbeat/", REPO_URLPREFIX);
	if(SEQUENCER.getHeartbeatData(challenge, hearbeatdata))
		return false;

	if (HTTPPOST(hearbeaturl, hearbeatdata) < 0)
		return false;
	// the server gives back "failed" or "ok"
	if(strncmp(httpresp, "2\r\nok", 5) >= 0)
		return true;
	else
		return false;
}

void Notifier::loop()
{
	bool advertised = registerServer();
	if (!advertised && servermode == SERVER_AUTO)
	{
		printf("using LAN mode, probably no internet users will be able to join your server!\n");
	}
	else if (!advertised && servermode == SERVER_INET)
	{
		printf("registration failed, exiting!\n");
		return;
	}

	//heartbeat
	while (!exit)
	{
		//every minute
#ifndef __WIN32__
		sleep(60);
#else
		Sleep(60*1000);
#endif

		if(advertised)
		{
			bool result = sendHearbeat();
			if (result) error_count=0;
			if(!result && servermode == SERVER_INET)
			{
				error_count++;
				if (error_count==5) 
				{
					logmsgf(LOG_ERROR,"heartbeat failed, exiting!");
					break;
				}
				else
				{
					logmsgf(LOG_WARN,"heartbeat failed, will try again");
				}
			}else if(!result && servermode != SERVER_INET)
				logmsgf(LOG_ERROR,"heartbeat failed!");
		}
	}

	unregisterServer();
}

int Notifier::HTTPGET(char* URL)
{
	int res=0;
	SWBaseSocket::SWBaseError error;
	SWInetSocket mySocket;

	if (mySocket.connect(80, REPO_SERVER, &error))
	{
		char query[2048];
		sprintf(query, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", URL, REPO_SERVER);
		//logmsgf(LOG_DEBUG,"Query to Master server: %s", query);
		if (mySocket.sendmsg(query, &error)<0)
		{
			logmsgf(LOG_ERROR,"Notifier: could not send request (%s)", error.get_error().c_str());
			res=-1;
		}
		int rlen=mySocket.recv(httpresp, 65536, &error);
		if (rlen>0 && rlen<65535) httpresp[rlen]=0;
		else
		{
			logmsgf(LOG_ERROR,"Notifier: could not get a valid response (%s)", error.get_error().c_str());
			res=-1;
		}
		//clean a bit
		char* pt=strstr(httpresp, "\r\n\r\n");
		pt+=4;
		strncpy(httpresp, pt, 65536);
		//logmsgf(LOG_DEBUG,"Response from Master server:'%s'", httpresp);
		// disconnect
		mySocket.disconnect();
	}
	else
	{
		logmsgf(LOG_ERROR,"Notifier: could not connect to the Master server (%s)", error.get_error().c_str());
		res=-1;
	}
	return res;
}

int Notifier::HTTPPOST(char* URL, char* data)
{
	int res=0;
	SWBaseSocket::SWBaseError error;
	SWInetSocket mySocket;

	if (mySocket.connect(80, REPO_SERVER, &error))
	{
		char query[2048];
		int len = (int)strnlen(data, 16000);
		sprintf(query, "POST %s HTTP/1.0\r\n" \
		               "Host: %s\r\n" \
					   "Content-Type: application/octet-stream\r\n" \
					   "Content-Length: %d\r\n"
					   "\r\n" \
					   "%s", \
					   URL, REPO_SERVER, len, data);
		logmsgf(LOG_DEBUG,"Query to Master server: %s", query);
		if (mySocket.sendmsg(query, &error)<0)
		{
			logmsgf(LOG_ERROR,"Notifier: could not send request (%s)", error.get_error().c_str());
			res=-1;
		}
		int rlen=mySocket.recv(httpresp, 65536, &error);
		if (rlen>0 && rlen<65535) httpresp[rlen]=0;
		else
		{
			logmsgf(LOG_ERROR,"Notifier: could not get a valid response (%s)", error.get_error().c_str());
			res=-1;
		}
		//clean a bit
		char* pt=strstr(httpresp, "\r\n\r\n");
		pt+=4;
		strncpy(httpresp, pt, 65536);
		logmsgf(LOG_DEBUG,"Response from Master server:'%s'", httpresp);
		// disconnect
		mySocket.disconnect();
	}
	else
	{
		logmsgf(LOG_ERROR,"Notifier: could not connect to the Master server (%s)", error.get_error().c_str());
		res=-1;
	}
	return res;
}
