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



UserAuth::UserAuth(std::string _challenge, int _trustlevel, std::string authFile)
{
    STACKLOG;
	challenge  = _challenge;
	trustlevel = _trustlevel;
	readConfig(authFile.c_str());
}

UserAuth::~UserAuth(void)
{
    STACKLOG;
}

int UserAuth::readConfig(const char* authFile)
{
	STACKLOG;
	FILE *f = fopen(authFile, "r");
	if (!f)
	{
		Logger::log(LOG_WARN, "Couldn't open the local authorizations file ('%s'). No authorizations were loaded.", authFile);
		return -1;
	}
	Logger::log(LOG_VERBOSE, "Reading the local authorizations file...");
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
		char user_nick[40] = "";
		int res = sscanf(line, "%d %s %s", &authmode, token, user_nick);
		if(res != 3 && res != 2)
		{
			Logger::log(LOG_ERROR, "error parsing authorizations file: " + std::string(line));
			continue;
		}
		
		// Not every auth mode is allowed to be set using the configuration file
		if(authmode & AUTH_RANKED) authmode &= ~AUTH_RANKED;
		if(authmode & AUTH_BANNED) authmode &= ~AUTH_BANNED;
		
		Logger::log(LOG_DEBUG, "adding entry to local auth cache, size: %d", local_auth.size());
		user_auth_pair_t p;
		p.first = authmode;
		p.second = widen(std::string(user_nick));
		local_auth[std::string(token)] = p;
	}
	Logger::log(LOG_INFO, "found %d auth overrides in the authorizations file!",  local_auth.size());
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

std::map< std::string, user_auth_pair_t > UserAuth::getAuthCache()
{
	return cache;
}

int UserAuth::setUserAuth(int flags, UTFString user_nick, std::string token)
{
	user_auth_pair_t p;
	p.first = flags;
	p.second = user_nick;
	local_auth[token] = p;
	return 0;
}

int UserAuth::sendUserEvent(std::string user_token, std::string type, std::string arg1, std::string arg2)
{
    STACKLOG;
	
	// Only contact the master server if we are allowed to do so
	if(trustlevel<=1) return 0;
	
	char url[2048];
	sprintf(url, "%s/userevent_utf8/?v=0&sh=%s&h=%s&t=%s&a1=%s&a2=%s", REPO_URLPREFIX, challenge.c_str(), user_token.c_str(), type.c_str(), arg1.c_str(), arg2.c_str());
	Logger::log(LOG_DEBUG, "UserAuth event to server: " + std::string(url));
	HttpMsg resp;
	if (HTTPGET(url, resp) < 0)
	{
		Logger::log(LOG_ERROR, "UserAuth event query result empty");
		return -1;
	}

	std::string body = resp.getBody();
	Logger::log(LOG_DEBUG,"UserEvent reply: " + body);

	return (body!="ok");
}

std::string UserAuth::getNewPlayernameByID(int id)
{
	char tmp[255] = "";
	sprintf(tmp, "Player%d", id);
	return std::string(tmp);
}

int UserAuth::resolve(std::string user_token, UTFString &user_nick, int clientid)
{
    STACKLOG;

	// There's alot of other info in the user token variable, but we don't need it here.
	// We only need the first 40 characters = the actual (encoded) token.
	user_token = user_token.substr(0,40);
	
	//check cache first
	if(cache.find(user_token) != cache.end())
	{
		// cache hit!
		user_nick = cache[user_token].second;
		return cache[user_token].first;
	}
	
	// initialize the authlevel on none = normal user
	int authlevel = AUTH_NONE;

	// Only contact the master-server if we're allowed to do so
	if(trustlevel>1)
	{
		std::string msg = "";
		UTFString resultNick = L"";
	
		// not found in cache or local_auth, get auth from masterserver
		char url[2048];
		sprintf(url, "%s/authuser_utf8/?c=%s&t=%s&u=%s", REPO_URLPREFIX, challenge.c_str(), user_token.c_str(), user_nick.asUTF8_c_str());
		Logger::log(LOG_DEBUG, "UserAuth query to server: " + std::string(url));
		HttpMsg resp;
		if (HTTPGET(url, resp) < 0)
		{
			Logger::log(LOG_ERROR, "UserAuth resolve query result empty");
			return AUTH_NONE;
		}

		std::string body = resp.getBody();
		Logger::log(LOG_DEBUG,"UserAuth reply: " + body);
		
		std::vector<std::string> args;
		strict_tokenize(body, args, "\t");

		if(args.size() < 2)
		{
			Logger::log(LOG_INFO,"UserAuth: invalid return value from server: " + body);
			return AUTH_NONE;
		}
		
		authlevel = atoi(args[0].c_str());
		resultNick = tryConvertUTF(args[1].c_str());
		if(args.size() > 2)
			msg = args[2];

		// debug output the auth status
		UTFString authst;
		if(authlevel & AUTH_NONE)   authst = authst + "N";
		if(authlevel & AUTH_ADMIN)  authst = authst + "A";
		if(authlevel & AUTH_MOD)    authst = authst + "M";
		if(authlevel & AUTH_RANKED) authst = authst + "R";
		if(authlevel & AUTH_BOT)    authst = authst + "B";
		if(authst.empty()) authst = "(none)";
		Logger::log(LOG_DEBUG, UTFString("User Auth Result: ") + authst + " / " + (resultNick) + " / " + tryConvertUTF(msg.c_str()));

		if(resultNick == L"error" || resultNick == L"reserved" || resultNick == L"notranked")
		{
			user_nick = widen(getNewPlayernameByID(clientid));
			Logger::log(LOG_DEBUG, UTFString("got new random name for player: ") + user_nick);
			return AUTH_NONE;
		}

		// returned name valid, use it
		user_nick = resultNick;
	}
	
	//then check for overrides in the authorizations file (server admins, etc)
	if(local_auth.find(user_token) != local_auth.end())
	{
		// local auth hit!
		// the stored nickname can be empty if no nickname is specified.
		if(!local_auth[user_token].second.empty())
			user_nick = local_auth[user_token].second;
		authlevel |= local_auth[user_token].first;
	}

	// cache result if ranked or higher
	if(authlevel > AUTH_NONE)
	{
		user_auth_pair_t p;
		p.first = authlevel;
		p.second = user_nick;
		
		Logger::log(LOG_DEBUG, "adding entry to remote auth cache, size: %d",  cache.size());
		cache[user_token] = p;
	}

	return authlevel;
}

int UserAuth::HTTPGET(const char* URL, HttpMsg &resp)
{
    STACKLOG;
	
	if(trustlevel<=1)
	{
		// If this happens, then you did something wrong in the server code
		Logger::log(LOG_ERROR, "userauth: tried to contact master server without permission. URL: %s", URL);
		return 0;
	}
	
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

