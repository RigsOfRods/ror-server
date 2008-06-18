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
}

UserAuth::~UserAuth(void)
{
    STACKLOG;
}

int UserAuth::resolve(std::string user_token, std::string &user_nick)
{
    STACKLOG;

	//check cache first
	if(cache[user_token] != "")
	{
		// cache hit!
		user_nick=cache[user_token];
		return 0;
	}

	// not found in cache, get auth
	char url[1024];
	sprintf(url, "%s/authuser/?c=%s&t=%s", REPO_URLPREFIX, challenge.c_str(), user_token.c_str());
	HttpMsg resp;
	if (HTTPGET(url, resp) < 0)
		return -1;

	std::string body = resp.getBody();
	if(body == "error")
		return -1;
	
	if(body == "notranked")
		return -2;

	cache[user_token] = body;
	user_nick=body;

	return 0;
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

