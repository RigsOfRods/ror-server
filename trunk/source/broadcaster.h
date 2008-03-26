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
#ifndef __Broadcaster_H__
#define __Broadcaster_H__

#include <pthread.h>
#include <SocketW.h>
#include "sequencer.h"

#define QUEUE_LENGTH 25


typedef struct
{
	unsigned int uid;
	int type;
	char data[MAX_MESSAGE_LENGTH];
	unsigned int datalen;
} queue_entry_t;

///TODO: Documents the broadcaster class
class Broadcaster
{
private:
	pthread_t thread;
	pthread_mutex_t queue_mutex;
	pthread_cond_t queue_cv;
	
	int id;
	SWInetSocket *sock;
	queue_entry_t queue[QUEUE_LENGTH];
	int queue_start; //RING BUFFER BABY!
	int queue_end;
	bool finish;
	char send_data[MAX_MESSAGE_LENGTH];
	bool alive;

public:
	Broadcaster();
	~Broadcaster(void);
	void reset(int pos, SWInetSocket *socky);
	void stop();
	void threadstart();
	void queueMessage(unsigned int uid, int type, char* data, unsigned int len);
};
#endif
