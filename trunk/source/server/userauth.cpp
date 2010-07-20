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
#include "utils.h"

#include <stdexcept>

#ifdef __GNUC__
#include <unistd.h>
#include <stdlib.h>
#endif

#include <cstdio>



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
		int authmode = AUTH_NONE;
		char token[256];
		int res = sscanf(line, "%d %s", &authmode, token);
		if(res != 2)
		{
			Logger::log(LOG_ERROR, "error parsing admins.txt file: " + std::string(line));
			continue;
		}
		Logger::log(LOG_DEBUG, "adding entry to local auth cache, size: %d", local_auth.size());
		local_auth[std::string(token)] = authmode;
	}
	Logger::log(LOG_INFO, "found %d auth overrides!",  local_auth.size());
	fclose (f);
	return 0;
}

int UserAuth::getAuthSize()
{
	return local_auth.size();
}

void UserAuth::clearCache()
{
	local_auth.clear();
}

std::map< std::string, std::pair<int, std::string> > UserAuth::getAuthCache()
{
	return cache;
}


int UserAuth::getUserModeByUserToken(std::string token)
{
	std::string nick;
	return resolve(token, nick);
}

int UserAuth::setUserAuth(std::string token, int flags)
{
	local_auth[token] = flags;
	return 0;
}

int UserAuth::sendUserEvent(std::string user_token, std::string type, std::string arg1, std::string arg2)
{
    STACKLOG;
	char url[1024];
	sprintf(url, "%s/userevent/?v=0&sh=%s&h=%s&t=%s&a1=%s&a2=%s", REPO_URLPREFIX, challenge.c_str(), user_token.c_str(), type.c_str(), arg1.c_str(), arg2.c_str());
	HttpMsg resp;
	if (HTTPGET(url, resp) < 0)
		return -1;

	std::string body = resp.getBody();
	Logger::log(LOG_DEBUG,"UserEvent reply: " + body);

	return (body!="ok");
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

	int authlevel = AUTH_NONE;
	std::string body = resp.getBody();
	Logger::log(LOG_DEBUG,"UserAuth reply: " + body);
	
	std::vector<std::string> args;
	strict_tokenize(body, args, "\t");

	if(args.size() != 2)
	{
		Logger::log(LOG_INFO,"UserAuth: invalid return value from server: " + body);
		return AUTH_NONE;
	}
	
	authlevel = atoi(args[0].c_str());
	user_nick = args[1];

	// check for local auth overrides (server admins, etc)
	if(local_auth.find(user_token) != local_auth.end())
		authlevel |= local_auth[user_token];

	// debug output the auth status
	char authst[10] = "";
	if(authlevel & AUTH_ADMIN) strcat(authst, "A");
	if(authlevel & AUTH_MOD) strcat(authst, "M");
	if(authlevel & AUTH_RANKED) strcat(authst, "R");
	if(authlevel & AUTH_BOT) strcat(authst, "B");
	if(authlevel != AUTH_NONE) Logger::log(LOG_INFO,"User Auth Flags: " + std::string(authst));

	// cache result
	std::pair< int, std::string > p;
	p.first = authlevel;
	p.second = user_nick;

	Logger::log(LOG_DEBUG, "adding entry to remote auth cache, size: %d",  cache.size());
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

