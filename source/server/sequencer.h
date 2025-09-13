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

#include "blacklist.h"
#include "prerequisites.h"
#include "rornet.h"
#include "spamfilter.h"
#include "json/json.h"
#include "enet/enet.h"
#include "dispatcher_enet.h"

#ifdef WITH_ANGELSCRIPT

#include "scriptmath3d/scriptmath3d.h" // angelscript addon

#endif //WITH_ANGELSCRIPT

#include "UnicodeStrings.h"

#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <map>
#include <thread>
#include <condition_variable>

// How many not-vehicles streams has every user by default? (e.g.: "default" and "chat" are not-vehicles streams)
// This is used for the vehicle-limit
#define NON_VEHICLE_STREAMS 2

// Specified by RoR; used for spawn-rate limit
#define STREAM_REG_TYPE_VEHICLE 0

#define SEQUENCER Sequencer::Instance()

#define VERSION __DATE__

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

struct stream_traffic_t {
    // normal bandwidth
    double bandwidthIncoming;
    double bandwidthOutgoing;
    double bandwidthIncomingLastMinute;
    double bandwidthOutgoingLastMinute;
    double bandwidthIncomingRate;
    double bandwidthOutgoingRate;

    // drop bandwidth
    double bandwidthDropIncoming;
    double bandwidthDropOutgoing;
    double bandwidthDropIncomingLastMinute;
    double bandwidthDropOutgoingLastMinute;
    double bandwidthDropIncomingRate;
    double bandwidthDropOutgoingRate;
};

class Client {
public:

    enum Status {
        STATUS_FREE = 0,
        STATUS_BUSY = 1,
        STATUS_USED = 2
    };

    enum Progress
    {
        INVALID,
        AWAITING_HELLO,
        AWAITING_USER_INFO,
        BEING_VERIFIED,
        PLAYING,
        DISCONNECTING
    };

    Client(Sequencer *sequencer, ENetPeer* peer, DispatcherENet* dispatcher_enet);

    void Disconnect();

    void QueueMessage(int msg_type, int client_id, unsigned int stream_id, unsigned int payload_len, const char *payload);
    void MessageReceived(ENetPacket* packet);

    void NotifyAllVehicles(Sequencer *sequencer);

    bool CheckSpawnRate(); //!< True if OK to spawn, false if exceeded maximum

    std::string GetIpAddress();

    bool IsBroadcasterDroppingPackets() const { return false; /*TODO delete after ENet is integrated*/  }

    bool IsReceivingData() const { return true; /*TODO delete after ENet is integrated*/ }

    Status GetStatus() const { return m_status; }

    int GetUserId() const { return static_cast<int>(user.uniqueid); }

    std::string GetUsername() const { return Str::SanitizeUtf8(user.username); }

    SpamFilter& GetSpamFilter() { return m_spamfilter; }

    void SetProgress(Progress v) { m_progress = v; }
    Progress GetProgress() { return m_progress; }

    RoRnet::UserInfo user;  //!< user information

    int drop_state;             // dropping outgoing packets?

    std::map<unsigned int, RoRnet::StreamRegister> streams;

    std::map<unsigned int, stream_traffic_t> streams_traffic;

private:
    ENetPeer* m_peer = nullptr;
    DispatcherENet* m_dispatcher_enet = nullptr;
    Progress m_progress = Progress::INVALID;
    Status m_status;
    SpamFilter m_spamfilter;
    Sequencer* m_sequencer;
    bool m_is_initialized;
    std::vector<std::chrono::system_clock::time_point> m_stream_reg_timestamps; //!< To limit spawn rate
    
};

struct WebserverClientInfo // Needed because Client cannot be trivially copied anymore due to presence of std::atomic<>
{
    WebserverClientInfo(Client* c):
        user (c->user),
        status(c->GetStatus()),
        ip_address(c->GetIpAddress()),
        streams(c->streams),
        streams_traffic(c->streams_traffic){
    }
    Client::Status GetStatus() const { return status; }
    std::string GetIpAddress() const { return ip_address; }

    RoRnet::UserInfo user;  //!< Copy of user information
    Client::Status status;
    std::string ip_address;
    std::map<unsigned int, RoRnet::StreamRegister> streams;
    std::map<unsigned int, stream_traffic_t> streams_traffic;
};

struct ban_t {
	unsigned int bid;			//!< id of ban, not the user id
    char ip[40];                //!< ip of banned client
    char nickname[RORNET_MAX_USERNAME_LEN];          //!< Username, this is what they are called to
    char bannedby_nick[RORNET_MAX_USERNAME_LEN];     //!< Username, this is what they are called to	
    char banmsg[256];           //!< why he got banned
};

struct report_t {
    unsigned int rid;           //!< id of report
    unsigned int cid;           //!< client id of the reported user, just in case
    char ip[40];                //!< ip of the reported user
    char nickname[RORNET_MAX_USERNAME_LEN];  //!< nickname of the reported user
    char reportedby_nick[RORNET_MAX_USERNAME_LEN]; //!< nickname of the reporter
    char reportmsg[256];        //!< reason for report
};

class Sequencer {
    friend class SpamFilter;
    friend class Client;
    friend class ServerScript;
    friend class Blacklist;
public:

    // Startup and shutdown
    Sequencer();
    void Initialize();
    void Close();

    // Synchronized public interface
    void registerClient(Client *client, RoRnet::UserInfo user);
    void queueMessage(int uid, int type, unsigned int streamid, const char *data, unsigned int len);
    void disconnectClient(int client_id, const char* error, bool isError = true, bool doScriptCallback = true);
    int getNumClients();
    void frameStepScripts(float dt);
    void GetHeartbeatUserList(Json::Value &out_array);
    void UpdateMinuteStats();
    int AuthorizeNick(std::string token, std::string &nickname);
    std::vector<WebserverClientInfo> GetClientListCopy();
    int getStartTime();


private:
    // Helpers (not thread safe - only call when clients-mutex is locked!)
    Client*                  FindClientById(unsigned int client_id);
    Client*                  getClient(int uid);
    void                     QueueClientForDisconnect(int client_id, const char *error, bool isError = true, bool doScriptCallback = true);
    void                     serverSay(std::string msg, int uid = -1, int type = 0);
    void                     sendMOTD(int id);
    void                     IntroduceNewClientToAllVehicles(Client *client);
    int                      sendGameCommand(int uid, std::string cmd);
    void                     printStats(); //! prints the Stats view, of who is connected and what slot they are in
    bool                     CheckNickIsUnique(std::string &nick);
    int                      GetFreePlayerColour();
    bool                     Kick(int to_kick_uid, int modUID, const char *msg = 0);
    bool                     Ban(int to_ban_uid, int modUID, const char *msg = 0);
    bool                     Report(int to_report_uid, int reporter_uid, const char *msg = 0);
    void                     SilentBan(int to_ban_uid, const char *msg = 0, bool doScriptCallback = true);
    void                     RecordBan(std::string const& ip_addr, std::string const& nickname, std::string const& by_nickname, std::string const& banmsg);
    void                     RecordReport(int to_report_uid, std::string const& ip_addr, std::string const& nickname, std::string const& by_nickname, std::string const& msg);
    bool                     IsBanned(const char *ip);
    bool                     UnBanIP(std::string ip_addr);
    bool                     UnBan(int bid);
    void                     streamDebug();
    std::vector<ban_t>       GetBanListCopy();
    void                     broadcastUserInfo(int uid);


    std::mutex m_clients_mutex;  //!< Protects: m_clients, m_script_engine, m_auth_resolver, m_bot_count
    ScriptEngine *m_script_engine;
    UserAuth *m_auth_resolver;
    int m_bot_count;      //!< Amount of registered bots on the server.
    unsigned int m_free_user_id;
    int m_start_time;
    Blacklist m_blacklist;

    std::vector<Client *> m_clients;
    std::vector<ban_t *> m_bans;
    std::vector<report_t *> m_reports;
};

