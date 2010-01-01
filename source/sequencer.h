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

// $LastChangedDate$
// $LastChangedRevision$
// $LastChangedBy$
// $HeadURL$
// $Id$
// $Rev$

#ifndef __Sequencer_H__
#define __Sequencer_H__

#include "rornet.h"
#include "scriptmath3d/scriptmath3d.h" // angelscript addon
#include "mutexutils.h"
#include <string>

#include <queue>
#include <vector>
#include <map>

class Broadcaster;
class Receiver;
class Listener;
class Notifier;
class UserAuth;
class SWInetSocket;
class ScriptEngine;

#define FREE 0
#define BUSY 1
#define USED 2

#define SEQUENCER Sequencer::Instance()

#define AUTH_NONE              0x00000000
#define AUTH_ADMIN             0x00000001
#define AUTH_RANKED            0x00000002
#define AUTH_MOD               0x00000004
#define AUTH_BOT               0x00000008
#define AUTH_BANNED            0x00000010

#define VERSION "$Rev$"

typedef struct stream_traffic_t
{
	double bandwidthIncoming;
	double bandwidthOutgoing;
	double bandwidthIncomingLastMinute;
	double bandwidthOutgoingLastMinute;
	double bandwidthIncomingRate;
	double bandwidthOutgoingRate;
} stream_traffic_t;

//! A struct to hold information about a client
struct client_t
{
    int status;                 //!< current status of the client, options are
                                //!< FREE, BUSY or USED
    Receiver* receiver;         //!< pointer to a receiver class, this
    Broadcaster* broadcaster;   //!< pointer to a broadcaster class
    SWInetSocket* sock;         //!< socket used to communicate with the client
    bool flow;                  //!< flag to see if the client should be sent
                                //!< data?
    char vehicle_name[140];     //!< name of the vehicle
    char nickname[32];          //!< Username, this is what they are called to
                                //!< other players
    unsigned int uid;           //!< userid
    unsigned int slotnum;       //!< slot number
    Vector3 position;           //!< position on the map?
    char uniqueid[60];          //!< users unique id
    
	int authstate;              //!< authenticated state
	int colournumber;           //!< player colour
	bool initialized;

	int beambuffersize;
	simple_beam_info *sbi;
	char ip_addr[16]; // do not use directly


	//things for the communication with the webserver below, not used in the main server code
	std::map<unsigned int, stream_register_t> streams;
	std::map<unsigned int, stream_traffic_t> streams_traffic;
};

struct ban_t
{
    unsigned int uid;           //!< userid
    char ip[40];                //!< ip of banned client
    char nickname[32];          //!< Username, this is what they are called to
    char bannedby_nick[32];     //!< Username, this is what they are called to	
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
    Notifier* notifier;     //!< registers and handles the master server
	UserAuth* authresolver; //!< authenticates users
    std::vector<client_t*> clients; //!< clients is a list of all the available 
    std::vector<ban_t*> bans; //!< list of bans
                            //!< client connections, it is allocated
    unsigned int fuid;      //!< next userid
    std::queue<client_t*> killqueue; //!< holds pointer for client deletion
    
    int startTime;
    unsigned short getPosfromUid(unsigned int uid);

protected:
    Sequencer();
    ~Sequencer();
    //! method to access the singleton instance
    static Sequencer* Instance();
    static Sequencer* mInstance;
	
	
	static int readFile(std::string filename, std::vector<std::string> &lines); //!< reads lines of a file
    
public:
    //!    initilize theSequencers information
    static void initilize();
    
    //! destructor call, used for clean up
    static void cleanUp();
    
    //! initilize client information
    static void createClient(SWInetSocket *sock, user_credentials_t *user);
    
    //! call to start the thread to disconnect clients from the server.
    static void killerthreadstart();
    
    //! queue client for disconenct
    static void disconnect(int pos, const char* error, bool isError=true);

	static void queueMessage(int pos, int type, unsigned int streamid, char* data, unsigned int len);
    static void enableFlow(int id);
    static int sendMOTD(int id);
    
    static void notifyRoutine();
    static void notifyAllVehicles(int id, bool lock=true);

	static UserAuth* getUserAuth();
	static ScriptEngine* getScriptEngine();

    static int getNumClients(); //! number of clients connected to this server
	static client_t *getClient(int uid);
	static int getHeartbeatData(char *challenge, char *hearbeatdata);
    //! prints the Stats view, of who is connected and what slot they are in
    static void printStats();
	static void updateMinuteStats();
    static void serverSay(std::string msg, int notto=-1, int type=0);
    static int sendGameCommand(int uid, std::string cmd);
    static void serverSayThreadSave(std::string msg, int notto=-1, int type=0);
	
	static bool checkNickUnique(char *nick);
	static int getFreePlayerColour();
	static int authNick(std::string token, std::string &nickname);

    static void  unregisterServer();

	static bool kick(int to_kick_uid, int modUID, const char *msg=0);
	static bool ban(int to_ban_uid, int modUID, const char *msg=0);
	static bool unban(int buid);
	static bool isbanned(const char *ip);
	static void streamDebug();

	static std::vector<client_t> getClients();
	static int getStartTime();

	static unsigned int connCrash, connCount;
};

#endif
