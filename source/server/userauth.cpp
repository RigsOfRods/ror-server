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

#include "userauth.h"

#include "config.h"
#include "rornet.h"
#include "logger.h"
#include "http.h"
#include "utils.h"

#include <stdexcept>
#include <cstdio>
#include <Poco/Dynamic/Struct.h>

#ifdef __GNUC__

#include <unistd.h>
#include <stdlib.h>

#endif

UserAuth::UserAuth(std::string authFile) {
    readConfig(authFile.c_str());
}

int UserAuth::readConfig(const char *authFile) {
    FILE *f = fopen(authFile, "r");
    if (!f) {
        Logger::Log(LOG_WARN, "Couldn't open the local authorizations file ('%s'). No authorizations were loaded.",
                    authFile);
        return -1;
    }
    Logger::Log(LOG_VERBOSE, "Reading the local authorizations file...");
    int linecounter = 0;
    while (!feof(f)) {
        char line[2048] = "";
        memset(line, 0, 2048);
        fgets(line, 2048, f);
        linecounter++;

        if (strnlen(line, 2048) <= 2)
            continue;

        if (line[0] == ';')
            // ignore comment lines
            continue;

        // this is setup mode (server)
        // strip line (newline char)
        char *ptr = line;
        while (*ptr) {
            if (*ptr == '\n') {
                *ptr = 0;
                break;
            }
            ptr++;
        }
        int authmode = RoRnet::AUTH_NONE;
        char token[256];
        const int NICK_LEN = 40;
        char user_nick[NICK_LEN] = "";
        int res = sscanf(line, "%d %s", &authmode, token);
        if (res != 2) {
            Logger::Log(LOG_ERROR, "error parsing authorizations file: " + std::string(line));
            continue;
        }

        // Seek to username
        char* c = line;
        int state = 0; // 0=authmode, 1=spaces, 2=token, 3=spaces, 4=nick
        while ((state != 4) && (*c != '\0')) {
            if      (state == 0 &&  isspace(*c)) { state = 1; ++c; }
            else if (state == 1 && !isspace(*c)) { state = 2; ++c; }
            else if (state == 2 &&  isspace(*c)) { state = 3; ++c; }
            else if (state == 3 && !isspace(*c)) { state = 4; }
            else { ++c; }
        }

        // Read username if present
        if (state == 4) {
            strncpy(user_nick, c, NICK_LEN);
            int rpos = strnlen(user_nick, NICK_LEN) - 1;
            while (isspace(user_nick[rpos])) {
                user_nick[rpos--] = '\0'; // Trim trailing whitespace
            }
        }

        // Not every auth mode is allowed to be set using the configuration file
        if (authmode & RoRnet::AUTH_RANKED) authmode &= ~RoRnet::AUTH_RANKED;
        if (authmode & RoRnet::AUTH_BANNED) authmode &= ~RoRnet::AUTH_BANNED;

        Logger::Log(LOG_DEBUG, "adding entry to local auth cache, size: %d", local_auth.size());
        user_auth_pair_t p;
        p.first = authmode;
        p.second = Str::SanitizeUtf8(user_nick);
        local_auth[std::string(token)] = p;
    }
    Logger::Log(LOG_INFO, "found %d auth overrides in the authorizations file!", local_auth.size());
    fclose(f);
    return 0;
}

int UserAuth::setUserAuth(int flags, std::string user_nick, std::string token) {
    user_auth_pair_t p;
    p.first = flags;
    p.second = user_nick;
    local_auth[token] = p;
    return 0;
}

int UserAuth::sendUserEvent(std::string user_token, std::string type, std::string arg1, std::string arg2) {
    /* #### DISABLED UNTIL SUPPORTED BY NEW MULTIPLAYER PORTAL ####

    // Only contact the master server if we are allowed to do so
    if(trustlevel<=1) return 0;
    
    char url[2048];
    sprintf(url, "%s/userevent_utf8/?v=0&sh=%s&h=%s&t=%s&a1=%s&a2=%s", REPO_URLPREFIX, challenge.c_str(), user_token.c_str(), type.c_str(), arg1.c_str(), arg2.c_str());
    Logger::Log(LOG_DEBUG, "UserAuth event to server: " + std::string(url));
    Http::Response resp;
    if (HTTPGET(url, resp) < 0)
    {
        Logger::Log(LOG_ERROR, "UserAuth event query result empty");
        return -1;
    }

    std::string body = resp.GetBody();
    Logger::Log(LOG_DEBUG,"UserEvent reply: " + body);

    return (body!="ok");

    */

    return -1;
}

int UserAuth::resolve(std::string user_token, std::string &user_nick, int clientid) {
    // initialize the authlevel on none = normal user
    int authlevel = RoRnet::AUTH_NONE;

    // contact the master server
    char url[512];
    sprintf(url, "/%s/users", Config::GetServerlistPath().c_str());
    Logger::Log(LOG_INFO, "Attempting user authentication (%s%s)", Config::GetServerlistHostC(), url);

    Poco::DynamicStruct data;
    data["username"] = user_nick;
    data["user_token"] = user_token;
    auto json_str = JsonToString(data);

    Http::Response resp;
    int result_code = Http::Request(Http::METHOD_GET,
                                    Config::GetServerlistHostC(), url, "application/json",
                                    json_str, &resp);

    // 200 means success!
    if (result_code == 200) {
        Logger::Log(LOG_INFO, "User authentication success, result code: %d", result_code);
        authlevel = RoRnet::AUTH_RANKED;
    } else {
        Logger::Log(LOG_INFO, "User authentication failed: %s, result code: %d",resp.GetBody().c_str(), result_code);
    }

    //then check for overrides in the authorizations file (server admins, etc)
    if (local_auth.find(user_token) != local_auth.end()) {
        // local auth hit!
        // the stored nickname can be empty if no nickname is specified.
        if (!local_auth[user_token].second.empty())
            user_nick = local_auth[user_token].second;
        authlevel |= local_auth[user_token].first;
    }

    return authlevel;
}

