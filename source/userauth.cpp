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
#include "userauth.h"
#include "sequencer.h"
#include "rornet.h"
#include "messaging.h"
#include "logger.h"

#include <stdexcept>

#ifdef __GNUC__
#include <unistd.h>
#endif



UserAuth::UserAuth(std::string _challenge)
{
    STACKLOG;
	challenge=_challenge;
	readConfig();
}

UserAuth::~UserAuth(void)
{
    STACKLOG;
}

int UserAuth::readConfig()
{
	STACKLOG;
	FILE *f = fopen(ADMINCONFIGFILE, "r");
	if (!f)
	{
		Logger::log(LOG_ERROR, "error opening admin configuration file '%s'", ADMINCONFIGFILE);
		return -1;
	}
	Logger::log(LOG_INFO, "reading admin configuration file...");
	int linecounter=0;
	while(!feof(f))
	{
		char line[2048] = "";
		memset(line, 0, 2048);
		fgets (line, 2048, f);
		linecounter++;
		
		if(strnlen(line, 2048) <= 2)
			continue;

		if(line[0] == ';')
			// ignore comment lines
			continue;
		
		// this is setup mode (server)
		// strip line (newline char)
		char *ptr = line;
		while(*ptr)
		{
			if(*ptr == '\n')
			{
				*ptr=0;
				break;
			}
			ptr++;
		}
		admin_entries.push_back(std::string(line));
	}
	Logger::log(LOG_INFO, "found %d admins!", admin_entries.size());
	fclose (f);
	return 0;
}

int UserAuth::getUserModeByUserToken(std::string token)
{
	std::string nick;
	return resolve(token, nick);
}

int UserAuth::resolve(std::string user_token, std::string &user_nick)
{
    STACKLOG;

	//check cache first
	if(cache.find(user_token) != cache.end())
	{
		// cache hit!
		user_nick = cache[user_token].second;
		return cache[user_token].first;
	}

	// not found in cache, get auth
	char url[1024];
	sprintf(url, "%s/authuser/?c=%s&t=%s", REPO_URLPREFIX, challenge.c_str(), user_token.c_str());
	HttpMsg resp;
	if (HTTPGET(url, resp) < 0)
		return -1;

	char nickname[40] = "";
	int authlevel = AUTH_NONE;
	std::string body = resp.getBody();
	Logger::log(LOG_DEBUG,"UserAuth reply: " + body);
	int res = sscanf(body.c_str(), "%d %s", &authlevel, nickname);
	if(res != 2)
	{
		Logger::log(LOG_INFO,"UserAuth: invalid return value from server: " + body);
		return authlevel;
	}
	
	if(authlevel == AUTH_NONE) Logger::log(LOG_INFO,"UserAuth: user " + user_nick + " has no auth flags!");
	if(authlevel & AUTH_RANKED) Logger::log(LOG_INFO,"UserAuth: user " + user_nick + " is ranked");
	if(authlevel & AUTH_ADMIN) Logger::log(LOG_INFO,"UserAuth: user " + user_nick + " is admin");
	if(authlevel & AUTH_MOD) Logger::log(LOG_INFO,"UserAuth: user " + user_nick + " is moderator");
	if(authlevel & AUTH_BOT) Logger::log(LOG_INFO,"UserAuth: user " + user_nick + " is bot");
	
	user_nick = std::string(nickname);

	// check for local admin flags
	// XXX: improve this to allow just any local auth level override!
	std::vector<std::string>::iterator it;
	for(it=admin_entries.begin(); it!=admin_entries.end(); it++)
	{
		if(token == *it)
			authlevel |= AUTH_ADMIN;
	}


	// cache result
	std::pair< int, std::string > p;
	p.first = authlevel;
	p.second = std::string(nickname);
	cache[user_token] = p;

	return authlevel;
}

int UserAuth::HTTPGET(const char* URL, HttpMsg &resp)
{
    STACKLOG;
	char httpresp[65536];  //!< http response from the master server
	int res=0;
	SWBaseSocket::SWBaseError error;
	SWInetSocket mySocket;

	if (mySocket.connect(80, REPO_SERVER, &error))
	{
		char query[2048];
		sprintf(query, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", URL, REPO_SERVER);
		Logger::log(LOG_DEBUG,"Query to Master server: %s", query);
		if (mySocket.sendmsg(query, &error)<0)
		{
			Logger::log(LOG_ERROR,"Notifier: could not send request (%s)", error.get_error().c_str());
			res=-1;
		}
		int rlen=mySocket.recv(httpresp, 65536, &error);
		if (rlen>0 && rlen<65535) httpresp[rlen]=0;
		else
		{
			Logger::log(LOG_ERROR,"Notifier: could not get a valid response (%s)", error.get_error().c_str());
			res=-1;
		}
		Logger::log(LOG_DEBUG,"Response from Master server:'%s'", httpresp);
		try
		{
			resp = HttpMsg(httpresp);
		}
		catch( std::runtime_error e)
		{
			Logger::log(LOG_ERROR, e.what() );
			res = -1;
		}
		// disconnect
		mySocket.disconnect();
	}
	else
	{
		Logger::log(LOG_ERROR,"Notifier: could not connect to the Master server (%s)", error.get_error().c_str());
		res=-1;
	}
	return res;
}

