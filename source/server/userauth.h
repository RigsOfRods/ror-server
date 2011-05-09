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
#ifndef __Userauth_H__
#define __Userauth_H__

#include "HttpMsg.h"
#include <vector>

class UserAuth
{
private:
	std::map< std::string, std::pair<int, std::string> > cache;
	int HTTPGET(const char* URL, HttpMsg &resp);
	std::string challenge;
	int trustlevel;
	std::map< std::string, std::pair<int, std::string> > local_auth;
	int readConfig(const char* authFile);
public:
	UserAuth(std::string challenge, int trustlevel, std::string authFile);
	~UserAuth();
	
	int getUserModeByUserToken(std::string token, int clientid);

	int resolve(std::string user_token, std::string &user_nick, int clientid);

	int getAuthSize();
	int setUserAuth(int flags, std::string user_nick, std::string token);

	int sendUserEvent(std::string user_token, std::string type, std::string arg1, std::string arg2);

	std::map< std::string, std::pair<int, std::string> > getAuthCache();
	void clearCache();

	std::string getNewPlayernameByID(int id);
};
#endif
