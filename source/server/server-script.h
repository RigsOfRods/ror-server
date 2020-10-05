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
along with "Rigs of Rods Server". If not,
see <http://www.gnu.org/licenses/>.
*/

#ifdef WITH_ANGELSCRIPT

#include "http.h"
#include "script-engine.h"

class ServerScript {
protected:
    ScriptEngine *mse;
    Sequencer *seq;

public:
    ServerScript(ScriptEngine *se, Sequencer *seq);

    ~ServerScript();

    void log(std::string &msg);

    void say(std::string &msg, int uid = -1, int type = 0);

    void kick(int kuid, std::string &msg);

    void ban(int kuid, std::string &msg);

    bool unban(int kuid);

    int playerChat(int uid, char *str);

    std::string getServerTerrain();

    int sendGameCommand(int uid, std::string cmd);

    std::string getUserName(int uid);

    void setUserName(int uid, const std::string &username);

    std::string getUserAuth(int uid);

    int getUserAuthRaw(int uid);

    void setUserAuthRaw(int uid, int authmode);

    int getUserColourNum(int uid);

    void setUserColourNum(int uid, int num);

    std::string getUserToken(int uid);

    std::string getUserVersion(int uid);

    std::string getUserIPAddress(int uid);

    int getUserPosition(int uid, Vector3 &v);

    int getNumClients();

    int getStartTime();

    int getTime();

    std::string get_version();

    std::string get_asVersion();

    std::string get_protocolVersion();

    void setCallback(const std::string &type, const std::string &func, void *obj, int refTypeId);

    void deleteCallback(const std::string &type, const std::string &func, void *obj, int refTypeId);

    void throwException(const std::string &message);

    unsigned int get_maxClients();

    std::string get_serverName();

    std::string get_IPAddr();

    unsigned int get_listenPort();

    int get_serverMode();

    std::string get_owner();

    std::string get_website();

    std::string get_ircServ();

    std::string get_voipServ();

    int rangeRandomInt(int from, int to);

    void broadcastUserInfo(int uid);

    Http::Response httpRequest( std::string method,
                                std::string host,
                                std::string url,
                                std::string content_type,
                                std::string payload);

    static void Register(asIScriptEngine* engine);
};

#endif // WITH_ANGELSCRIPT
