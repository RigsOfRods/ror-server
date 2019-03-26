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
#include "receiver.h"
#include "json/json.h"

#ifdef WITH_ANGELSCRIPT

#include "scriptmath3d/scriptmath3d.h" // angelscript addon

#endif //WITH_ANGELSCRIPT

#include "UnicodeStrings.h"

#include <queue>
#include <vector>
#include <map>



// How many not-vehicles streams has every user by default? (e.g.: "default" and "chat" are not-vehicles streams)
// This is used for the vehicle-limit
#define NON_VEHICLE_STREAMS 2

#define SEQUENCER Sequencer::Instance()

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

    Client(Sequencer *sequencer, SWInetSocket *socket);

    void StartThreads();

    void Disconnect();

    void QueueMessage(int msg_type, int client_id, unsigned int stream_id, unsigned int payload_len, const char *payload);

    void NotifyAllVehicles(Sequencer *sequencer);

    std::string GetIpAddress();

    SWInetSocket *GetSocket() { return m_socket; }

    bool IsBroadcasterDroppingPackets() const { return m_broadcaster.IsDroppingPackets(); }

    void SetReceiveData(bool val) { m_is_receiving_data = val; }

    bool IsReceivingData() const { return m_is_receiving_data; }

    Status GetStatus() const { return m_status; }

    RoRnet::UserInfo user;  //!< user information

    int drop_state;             // dropping outgoing packets?

    //things for the communication with the webserver below, not used in the main server code
    std::map<unsigned int, RoRnet::StreamRegister> streams;
    std::map<unsigned int, stream_traffic_t> streams_traffic;

private:
    SWInetSocket *m_socket;
    Receiver m_receiver;
    Broadcaster m_broadcaster;
    Status m_status;
    bool m_is_receiving_data;
    bool m_is_initialized;
};

struct ban_t {
    unsigned int uid;           //!< userid
    char ip[40];                //!< ip of banned client
    char nickname[RORNET_MAX_USERNAME_LEN];          //!< Username, this is what they are called to
    char bannedby_nick[RORNET_MAX_USERNAME_LEN];     //!< Username, this is what they are called to	
    char banmsg[256];           //!< why he got banned
};

void *LaunchKillerThread(void *);

class Sequencer {
    friend void *LaunchKillerThread(void *);

public:

    Sequencer();

    void Initialize();

    //! destructor call, used for clean up
    void Close();

    //! initilize client information
    void createClient(SWInetSocket *sock, RoRnet::UserInfo user);

    //! call to start the thread to disconnect clients from the server.
    void killerthreadstart();

    //! queue client for disconenct
    void disconnect(int pos, const char *error, bool isError = true, bool doScriptCallback = true);

    void queueMessage(int uid, int type, unsigned int streamid, char *data, unsigned int len);

    void enableFlow(int id);

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

    std::vector<Client> GetClientListCopy();

    static unsigned int connCrash, connCount;

private:
    pthread_t m_killer_thread;  //!< thread to handle the killing of clients
    Condition m_killer_cond;    //!< wait condition that there are clients to kill
    Mutex m_killer_mutex;   //!< mutex used for locking access to the killqueue
    Mutex m_clients_mutex;  //!< mutex used for locking access to the clients array
    ScriptEngine *m_script_engine;
    UserAuth *m_auth_resolver;
    int m_bot_count;      //!< Amount of registered bots on the server.
    unsigned int m_free_user_id;
    int m_start_time;

    std::queue<Client *> m_kill_queue; //!< holds pointer for client deletion
    std::vector<Client *> m_clients;
    std::vector<ban_t *> m_bans;

    Client *FindClientById(unsigned int client_id);
};

