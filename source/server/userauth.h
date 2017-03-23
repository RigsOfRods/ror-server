/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "http.h"
#include "UnicodeStrings.h"

#include <vector>

typedef std::pair<int, std::string> user_auth_pair_t;

class UserAuth
{
private:
    std::map< std::string, user_auth_pair_t > cache;
    int HTTPGET(const char* URL, Http::Response &resp);
    std::string challenge;
    int trustlevel;
    std::map< std::string, user_auth_pair_t > local_auth;
    int readConfig(const char* authFile);
public:
    UserAuth(std::string challenge, int trustlevel, std::string authFile);
    ~UserAuth();
    
    int resolve(std::string user_token, std::string &user_nick, int clientid);

    int getAuthSize();
    int setUserAuth(int flags, std::string user_nick, std::string token);

    int sendUserEvent(std::string user_token, std::string type, std::string arg1, std::string arg2);

    std::map< std::string,  user_auth_pair_t> getAuthCache();
    void clearCache();

    std::string getNewPlayernameByID(int id);
};

