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

typedef struct
{
	int status;
	Receiver* receiver;
	Broadcaster* broadcaster;
	SWInetSocket* sock;
	bool flow;
	char vehicle_name[140];
	char nickname[32];
	unsigned int uid;
	Vector3 position;
} client_t;

class Sequencer
{
private:
	pthread_t killerthread;
	pthread_mutex_t killer_mutex;
	pthread_cond_t killer_cv;
	pthread_mutex_t clients_mutex;
	Listener *listener;
	client_t *clients;
	int maxclients;
	unsigned int fuid;
	int killqueue[256];
	int freekillqueue;
	int servermode;

public:
	Sequencer(char *pubip, int listenport, char* servname, char* terrname, int max_clients, int servermode, char *password);
	~Sequencer(void);
	void createClient(SWInetSocket *sock, char* name);
	void killerthreadstart();
	void disconnect(int pos, char* error);
	void queueMessage(int pos, int type, char* data, unsigned int len);
	void notifyRoutine();
	void enableFlow(int id);
	void notifyAllVehicles(int id);
	int getNumClients();
	int getHeartbeatData(char *challenge, char *hearbeatdata);
	void printStats();
	Notifier *notifier;
};

#endif
