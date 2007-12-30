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

#include <iostream>

#include "SocketW.h"
#include "listener.h"
#include "receiver.h"
#include "broadcaster.h"
#include "notifier.h"
#include "Vector3.h"
class Broadcaster;
class Receiver;
class Listener;
class Notifier;

#define FREE 0
#define BUSY 1
#define USED 2

#define SEQUENCER Sequencer::Instance()

///A struct to hold information about a client
typedef struct
{
	/// current status of the client, options are FREE, BUSY or USED
	int status;
	/// pointer to a receiver class, this 
	Receiver* receiver;
	/// pointer to a broadcaster class
	Broadcaster* broadcaster;
	/// socket used to communicate with the client
	SWInetSocket* sock;
	/// flag to see if the client should be sent data?
	bool flow;
	/// name of the vehicle
	char vehicle_name[140];
	/// Username, this is what they are called to other players
	char nickname[32];
	/// userid
	unsigned int uid;
	/// position on the map?
	Vector3 position;
	/// users unique id
	char uniqueid[60];
} client_t;

class Sequencer
{
private:
	pthread_t killerthread;
	pthread_cond_t killer_cv;
	/// mutex used for locking access to the killqueue
	pthread_mutex_t killer_mutex;
	/// mutex used for locking access to the clients array
	pthread_mutex_t clients_mutex;

	Listener *listener;
	/**
	 *  @brief clients is a list of all the available client connections, it is allocated 
	 */
	client_t *clients;
	/// maximum number of clients allowed to connect to the server.
	int maxclients;
	unsigned int fuid;
	/** 
	 *  @brief killqueue is a queue of clients to be killed. 
	 *  killqueue is a queue of clients to be removed from the server.
	 *  it is an array of integers, where each integer is a position in the
	 *  client list.
	 */
	int killqueue[256];
	/**
	 *  @brief freekillqueue holds the number of spots available in the killqueue
	 */
	int freekillqueue;
	int servermode;
	char terrainName[255];
	char serverPassword[21];
	bool pwProtected;

protected:
	Sequencer(){}
	~Sequencer(){}

	static Sequencer* TheInstance;
	
public:
	/// method to access the singleton instance
	static Sequencer& Instance();
	///	initilize theSequencers information
	void initilize(char *pubip, int listenport, char* servname, char* terrname, int max_clients, int servermode, char *password);
	/// destructor call, used for clean up
	void cleanUp();
	/// initilize client information
	void createClient(SWInetSocket *sock, user_credentials_t *user);
	/// call to start the thread to disconnect clients from the server.
	void killerthreadstart();
	/// call to initiate the disconnect processes for a client.
	void disconnect(int pos, char* error);
	void queueMessage(int pos, int type, char* data, unsigned int len);
	void enableFlow(int id);
	void notifyRoutine();
	void notifyAllVehicles(int id);
	/// returns the number of clients connected to this server
	int getNumClients();
	int getHeartbeatData(char *challenge, char *hearbeatdata);
	/// prints the Stats view, of who is connected and what slot they are in
	void printStats();

	char *getTerrainName() { return terrainName; };
	bool isPasswordProtected() { return pwProtected; };
	char *getServerPasswordHash() { return serverPassword; };
	
	Notifier *notifier;
};

#endif
