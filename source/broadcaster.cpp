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

#include "broadcaster.h"
#include "logger.h"
#include "SocketW.h"
#include <map>

void *s_brthreadstart(void* vid)
{
	STACKLOG;
	Broadcaster* instance = ((Broadcaster*)vid);
	instance->threadstart();

	// check if are expecting to exit, if not running will still be true
	// if so wait for the join request
	if( instance->running )
	{
		MutexLocker scoped_lock( instance->queue_mutex );
		instance->running = false;
		instance->queue_mutex.wait( instance->queue_cv );
	}
#ifdef WIN32
	Logger::log( LOG_DEBUG, "broadcaster thread %u:%u is exiting",
		(unsigned int) &pthread_self().p, ThreadID::getID() );
#endif
	return NULL;
}
Broadcaster::Broadcaster()
:   id( 0 ), sock( NULL ), running( false )
{
	STACKLOG;
}

Broadcaster::~Broadcaster()
{
	STACKLOG;
}

void Broadcaster::reset(int uid, SWInetSocket *socky,
		void (*disconnect_func)(int, const char*, bool),
		int (*sendmessage_func)(SWInetSocket*, int, int, unsigned int, unsigned int, const char*) )
{
	STACKLOG;
	id          = uid;
	sock        = socky;
	running     = true;
	disconnect  = disconnect_func;
	sendmessage = sendmessage_func;

	// always clear to free up memory
	msg_queue.clear();

	// we've got a new client, release the signal
	//start a listener thread
	pthread_create(&thread, NULL, s_brthreadstart, this);
}

void Broadcaster::stop()
{
	STACKLOG;
	queue_mutex.lock();
	running = false;
	queue_cv.signal();
	queue_mutex.unlock();
#ifdef WIN32
	Logger::log( LOG_DEBUG, "joining with broadcaster thread: %u",
			(unsigned int) &thread.p);
#endif
	pthread_join( thread, NULL );
}

void Broadcaster::threadstart()
{
	STACKLOG;
	queue_entry_t msg;
	Logger::log( LOG_DEBUG, "broadcaster thread %u owned by uid %d", ThreadID::getID(), id);
	while( running )
	{
		{   // define a new scope and use a scope lock
			MutexLocker scoped_lock( queue_mutex );
			while( msg_queue.empty() && running) {
				queue_mutex.wait( queue_cv );
			}
			if( !running ) return;

			//pop stuff
			msg = msg_queue.front();
			msg_queue.pop_front();
		}   // unlock the mutex

		//Send message
		// TODO WARNING THE SOCKET IS NOT PROTECTED!!!
		if( sendmessage( sock, msg.type, msg.uid, msg.streamid, msg.datalen, msg.data ) )
		{
			disconnect(id, "Broadcaster: Send error", true);
			return;
		}
	}
}

//this is called all the way from the receiver threads, we should process this swiftly
//and keep in mind that it is called crazily and concurently from lots of threads
//we MUST copy the data too
//also, this function can be called by threads owning clients_mutex !!!
void Broadcaster::queueMessage(int type, int uid, unsigned int streamid, unsigned int len, const char* data)
{
	STACKLOG;
	if( !running ) return;
	// for now lets just queue msgs in the order received to make things simple
	queue_entry_t msg = { type, uid, streamid, "", len};
	memset( msg.data, 0, MAX_MESSAGE_LENGTH );
	memcpy( msg.data, data, len );

	MutexLocker scoped_lock( queue_mutex );

	// we will limit the entries in this queue
	
	// soft limit: we start dropping data packages
	if(msg_queue.size() > (size_t)queue_soft_limit && type == MSG2_STREAM_DATA)
	{
		Logger::log( LOG_DEBUG, "broadcaster queue soft full: thread %u owned by uid %d", ThreadID::getID(), id);
		return;
	}

	// hard limit drop anything, otherwise we would need to run through the queue and search and remove
	// data packages, which is not really feasible
	if(msg_queue.size() > (size_t)queue_hard_limit)
	{
		Logger::log( LOG_DEBUG, "broadcaster queue hard full: thread %u owned by uid %d", ThreadID::getID(), id);
		debugMessageQueue();
		return;
	}
	
	msg_queue.push_back( msg );
	//signal the thread that new data is waiting to be sent
	queue_cv.signal();

}

void Broadcaster::debugMessageQueue()
{
	// IMPORTANT: assumes we are still locked
	int msgsize = msg_queue.size();
	std::map < int, int > types;
	std::map < int, int >::iterator it;
	for(int i = 0; i < msgsize; i++)
	{
		int type = msg_queue.at(i).type;
		if(types.find(type) == types.end())
			types[type] = 0;
		
		types[type] += 1;
	}
	Logger::log( LOG_DEBUG, "broadcaster queue debug (thread %u owned by uid %d)", ThreadID::getID(), id);
	
	for(it = types.begin(); it != types.end() ; it++)
		Logger::log( LOG_DEBUG, " * message type %03d : %03d times out of %03d ( %0.2f %%)", it->first, it->second, msgsize, ((float)it->second / (float)msgsize) * 100.0f);

}

int Broadcaster::getMessageQueueSize()
{
	return msg_queue.size();
}