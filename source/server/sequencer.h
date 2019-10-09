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

#include "prerequisites.h"
#include "rornet.h"
#include "mutexutils.h"
#include "broadcaster.h"
#include "client.h"
#include "json/json.h"

#ifdef WITH_ANGELSCRIPT

#include "scriptmath3d/scriptmath3d.h" // angelscript addon

#endif //WITH_ANGELSCRIPT

#include "UnicodeStrings.h"

#include <queue>
#include <vector>
#include <map>

#include <event2/bufferevent.h>

// How many not-vehicles streams has every user by default? (e.g.: "default" and "chat" are not-vehicles streams)
// This is used for the vehicle-limit
#define NON_VEHICLE_STREAMS 2

#define VERSION "$Rev$"

// This is used to define who says it, when the server says something
enum serverSayType {
    FROM_SERVER = 0,
    FROM_HOST,
    FROM_MOTD,
    FROM_RULES
};

enum broadcastType {
// order: least restrictive to most restrictive!
            BROADCAST_AUTO = -1,  // Do not edit the publishmode (for scripts only)
    BROADCAST_ALL,        // broadcast to all clients including sender
    BROADCAST_NORMAL,     // broadcast to all clients except sender
    BROADCAST_AUTHED,     // broadcast to authed users (bots)
    BROADCAST_BLOCK       // no broadcast
};

// constant for functions that receive an uid for sending something
static const int TO_ALL = -1;

struct WebserverClientInfo // Needed because Client cannot be trivially copied anymore due to presence of std::atomic<>
{
    WebserverClientInfo(Client* c):
        user (c->user),
        ip_address(c->GetIpAddress()),
        streams(c->streams),
        streams_traffic(c->streams_traffic){
    }
    std::string GetIpAddress() const { return ip_address; }

    RoRnet::UserInfo user;  //!< Copy of user information
    std::string ip_address;
    std::map<unsigned int, RoRnet::StreamRegister> streams;
    std::map<unsigned int, stream_traffic_t> streams_traffic;
};

struct ban_t {
    unsigned int uid;           //!< userid
    char ip[40];                //!< ip of banned client
    char nickname[RORNET_MAX_USERNAME_LEN];          //!< Username, this is what they are called to
    char bannedby_nick[RORNET_MAX_USERNAME_LEN];     //!< Username, this is what they are called to	
    char banmsg[256];           //!< why he got banned
};

class Sequencer {
public:

    Sequencer();

    void Initialize();

    //! destructor call, used for clean up
    void Close();

    //! initilize client information
    void createClient(kissnet::tcp_socket socket, RoRnet::UserInfo user);

    void QueueClientForDisconnect(int client_id, const char *error, bool isError = true, bool doScriptCallback = true);

    void queueMessage(int uid, int type, unsigned int streamid, char *data, unsigned int len);

    int sendMOTD(int id);

    void IntroduceNewClientToAllVehicles(Client *client);

    UserAuth *getUserAuth();

    int getNumClients(); //! number of clients connected to this server
    Client *getClient(int uid);

    void GetHeartbeatUserList(Json::Value &out_array);

    //! prints the Stats view, of who is connected and what slot they are in
    void printStats();

    void UpdateMinuteStats();

    void serverSay(std::string msg, int notto = -1, int type = 0);

    int sendGameCommand(int uid, std::string cmd);

    void serverSayThreadSave(std::string msg, int notto = -1, int type = 0);

    bool CheckNickIsUnique(std::string &nick);

    int GetFreePlayerColour();

    int AuthorizeNick(std::string token, std::string &nickname);

    bool Kick(int to_kick_uid, int modUID, const char *msg = 0);

    bool Ban(int to_ban_uid, int modUID, const char *msg = 0);

    void SilentBan(int to_ban_uid, const char *msg = 0, bool doScriptCallback = true);

    bool UnBan(int buid);

    bool IsBanned(const char *ip);

    void streamDebug();

    int getStartTime();

    void broadcastUserInfo(int uid);

    std::vector<WebserverClientInfo> GetClientListCopy();

private:
    Mutex m_clients_mutex;  //!< Protects: m_clients, m_script_engine, m_auth_resolver, m_bot_count
    ScriptEngine *m_script_engine;
    UserAuth *m_auth_resolver;
    int m_bot_count;      //!< Amount of registered bots on the server.
    unsigned int m_free_user_id;
    int m_start_time;

    std::vector<Client *> m_clients;
    std::vector<ban_t *> m_bans;

    Client *FindClientById(unsigned int client_id);
};

