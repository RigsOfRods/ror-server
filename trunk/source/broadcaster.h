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
#include <queue>
#include "rornet.h"
#include "mutexutils.h"
class SWInetSocket;

struct queue_entry_t
{
	int uid;
	int type;
	char data[MAX_MESSAGE_LENGTH];
	unsigned int datalen;
	unsigned int streamid;
};

///TODO: Documents the broadcaster class
class Broadcaster
{
private:
	pthread_t thread;
	Mutex queue_mutex;
	Condition queue_cv;
	
	int id;
	SWInetSocket *sock; // this needs to go away
	std::queue<queue_entry_t> msg_queue;
	
	bool running;
	void (*disconnect)(int, const char*, bool);
	int (*sendmessage)(SWInetSocket *socket, int type, int source, unsigned int streamid, unsigned int len, const char* content);

	 void threadstart();
	 friend void* s_brthreadstart(void* vid);

public:
	Broadcaster();
	~Broadcaster(void);
	/**
	 * @param[in] uid   client id whom owns this broadcaster instance
	 * @param[in] socky clients sockets pointer
	 * @param[in] disconnect callback for disconnecting the client
	 * @param[in] sendmessage callback for send a message
	 */
	void reset(int uid, SWInetSocket *socky,
			void (*disconnect)(int uid, const char*, bool),
			int (*sendmessage)(SWInetSocket *socket, int type,
					int source, unsigned int streamid, unsigned int len, const char* content) );
	void stop();
	/**
	 * @param[in] uid  uid of the client sending the data??
	 * @param[in] type Type of message being sent
	 * @param[in] data the actually message being sent
	 * @param[in] len  length of data in bytes
	 */
	void queueMessage(int uid, int type, unsigned int streamid, unsigned int len, const char* data);
};
#endif
