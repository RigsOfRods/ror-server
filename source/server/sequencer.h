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

#include "rornet.h"
#include "notifier.h"
#include "mutexutils.h"
#include "UTFString.h"
#ifdef WITH_ANGELSCRIPT
#include "scriptmath3d/scriptmath3d.h" // angelscript addon
#endif //WITH_ANGELSCRIPT

#include <string>
#include <queue>
#include <vector>
#include <map>

class Broadcaster;
class Receiver;
class Listener;
class UserAuth;
class SWInetSocket;
class ScriptEngine;

#define FREE 0
#define BUSY 1
#define USED 2

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

typedef struct stream_traffic_t
{
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
} stream_traffic_t;

//! A struct to hold information about a client
struct client_t
{
    user_info_t user;  //!< user information
    int status;                 //!< current status of the client, options are
                                //!< FREE, BUSY or USED
    Receiver* receiver;         //!< pointer to a receiver class, this
    Broadcaster* broadcaster;   //!< pointer to a broadcaster class
    SWInetSocket* sock;         //!< socket used to communicate with the client
    bool flow;                  //!< flag to see if the client should be sent
                                //!< data?
    bool initialized;
    char ip_addr[16];           // do not use directly

    int drop_state;             // dropping outgoing packets?

    //things for the communication with the webserver below, not used in the main server code
    std::map<unsigned int, stream_register_t> streams;
    std::map<unsigned int, stream_traffic_t> streams_traffic;
};

struct ban_t
{
    unsigned int uid;           //!< userid
    char ip[40];                //!< ip of banned client
    char nickname[MAX_USERNAME_LEN];          //!< Username, this is what they are called to
    char bannedby_nick[MAX_USERNAME_LEN];     //!< Username, this is what they are called to	
    char banmsg[256];           //!< why he got banned
};

class Sequencer
{
private:
    pthread_t killerthread; //!< thread to handle the killing of clients
    Condition killer_cv;    //!< wait condition that there are clients to kill
    Mutex killer_mutex;     //!< mutex used for locking access to the killqueue
    Mutex clients_mutex;    //!< mutex used for locking access to the clients array
    
    Listener* listener;     //!< listens for incoming connections
    ScriptEngine* script;     //!< listens for incoming connections
    Notifier notifier;     //!< registers and handles the master server
    UserAuth* authresolver; //!< authenticates users
    std::vector<client_t*> clients; //!< clients is a list of all the available 
    std::vector<ban_t*> bans; //!< list of bans
                            //!< client connections, it is allocated
    unsigned int fuid;      //!< next userid
    std::queue<client_t*> killqueue; //!< holds pointer for client deletion
    int botCount;           //!< Amount of registered bots on the server.
    
    int startTime;
    unsigned short getPosfromUid(unsigned int uid);

public:

    Sequencer();
    ~Sequencer();

    void initialize(Listener* listener);
    void activateUserAuth();
    void registerServer();

    //! destructor call, used for clean up
    void cleanUp();
    
    //! initilize client information
    void createClient(SWInetSocket *sock, user_info_t  user);
    
    //! call to start the thread to disconnect clients from the server.
    void killerthreadstart();
    
    //! queue client for disconenct
    void disconnect(int pos, const char* error, bool isError=true, bool doScriptCallback=true);

    void queueMessage(int pos, int type, unsigned int streamid, char* data, unsigned int len);
    void enableFlow(int id);
    int sendMOTD(int id);
    
    void notifyRoutine();
    void notifyAllVehicles(int id, bool lock=true);

    UserAuth* getUserAuth();
    ScriptEngine* getScriptEngine();
    Notifier *getNotifier();

    int getNumClients(); //! number of clients connected to this server
    client_t *getClient(int uid);
    int getHeartbeatData(char *challenge, char *hearbeatdata);
    //! prints the Stats view, of who is connected and what slot they are in
    void printStats();
    void updateMinuteStats();
    void serverSay(std::string msg, int notto=-1, int type=0);
    int sendGameCommand(int uid, std::string cmd);
    void serverSayThreadSave(std::string msg, int notto=-1, int type=0);
    
    bool CheckNickIsUnique(UTFString &nick);
    int GetFreePlayerColour();
    int AuthorizeNick(std::string token, UTFString &nickname);

    bool Kick(int to_kick_uid, int modUID, const char *msg=0);
    bool Ban(int to_ban_uid, int modUID, const char *msg=0);
    void SilentBan(int to_ban_uid, const char *msg=0, bool doScriptCallback=true);
    bool UnBan(int buid);
    bool IsBanned(const char *ip);
    void streamDebug();

    std::vector<client_t> GetClientList();
    int getStartTime();
    void broadcastUserInfo(int uid);

    static unsigned int connCrash, connCount;

};

