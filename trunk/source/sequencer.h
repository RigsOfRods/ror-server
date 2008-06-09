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
#ifndef __Sequencer_H__
#define __Sequencer_H__

#include "rornet.h"
#include "Vector3.h"
#include "mutexutils.h"
#include <string>

#ifdef NCURSES
#include "curses.h"
#endif

class Broadcaster;
class Receiver;
class Listener;
class Notifier;
class SWInetSocket;

#define FREE 0
#define BUSY 1
#define USED 2

#define SEQUENCER Sequencer::Instance()

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
    Vector3 position;           //!< position on the map?
    char uniqueid[60];          //!< users unique id
    int rconretries;            //!< rcon password retries
    int rconauth;               //!< rcon authenticated mode
};

class Sequencer
{
private:
    pthread_t killerthread; //!< thread to handle the killing of clients
    Condition killer_cv;    //!< wait condition that there are clients to kill
    Mutex killer_mutex;     //!< mutex used for locking access to the killqueue
    Mutex clients_mutex;    //!< mutex used for locking access to the clients array
    
    Listener* listener;     //!< 
    Notifier* notifier;     //!<
    client_t* clients;      //!< clients is a list of all the available client
                            //!< connections, it is allocated
    int maxclients;         //!< maximum number of clients allowed to connect to
                            //!< the server.
    unsigned int fuid;      //!< next userid
    int killqueue[256];     //!< which position in the queue to kill
    int freekillqueue;      //!< freekillqueue holds the number of spots
                            //!< available in the killqueue
    int servermode;
    char terrainName[255];
    char serverPassword[41];
    bool pwProtected;
    
    char rconPassword[41];
    bool rconenabled;
    bool isSandbox;
    int startTime;
    unsigned short getPosfromUid(const unsigned int& uid);

protected:
    Sequencer();
    ~Sequencer();
    //! method to access the singleton instance
    static Sequencer* Instance();
    static Sequencer* mInstance;
    
public:
    //!    initilize theSequencers information
    static void initilize(char *pubip, int listenport, char* servname,
    		char*terrname, int max_clients, int servermode, char *password,
    		char *rconpassword, bool guimode);
    
    //! destructor call, used for clean up
    static void cleanUp();
    
    //! initilize client information
    static void createClient(SWInetSocket *sock, user_credentials_t *user);
    
    //! call to start the thread to disconnect clients from the server.
    static void killerthreadstart();
    
    //! queue client for disconenct
    static void disconnect(int pos, char* error);
    static void queueMessage(int pos, int type, char* data, unsigned int len);
    static void enableFlow(int id);
    
    static void notifyRoutine();
    static void notifyAllVehicles(int id);
    
    static int getNumClients(); //! number of clients connected to this server
    static int getHeartbeatData(char *challenge, char *hearbeatdata);
    //! prints the Stats view, of who is connected and what slot they are in
    static void printStats();
    static void serverSay(std::string msg, int notto=-1, int type=0);

    static char* getTerrainName();
    static bool  isPasswordProtected();
    static char* getServerPasswordHash();
    static void  unregisterServer();

#ifdef NCURSES
private:
    bool guimode;
    
    WINDOW *win_info;
    WINDOW *win_slots;
    WINDOW *win_log;
    WINDOW *win_chat;
    
public:
    static bool getGUIMode();
    static WINDOW *getLogWindow();
#endif    
};

#endif