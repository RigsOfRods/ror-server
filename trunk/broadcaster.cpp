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

void *s_brthreadstart(void* vid)
{
	((Broadcaster*)vid)->threadstart();
	return NULL;
}


Broadcaster::Broadcaster(Sequencer *seq)
{
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_cond_init(&queue_cv, NULL);
	queue_start=0;
	queue_end=0;
	sequencer=seq;
	id=0;
	sock=NULL;
	finish=false;
	alive=false;
}

Broadcaster::~Broadcaster(void)
{
	stop();
	logmsgf(LOG_DEBUG,"Cond destroy");
	pthread_cond_destroy(&queue_cv); //this can block in certains situations!
	logmsgf(LOG_DEBUG,"Mutex destroy");
	pthread_mutex_destroy(&queue_mutex);
	logmsgf(LOG_DEBUG,"Returning");
}

void Broadcaster::reset(int pos, SWInetSocket *socky)
{
	pthread_mutex_lock(&queue_mutex);
	if (alive)
	{
		logmsgf(LOG_ERROR,"Whoa, broadcaster is still alive!");
		pthread_mutex_unlock(&queue_mutex);
		return;
	}
	pthread_mutex_unlock(&queue_mutex);
	queue_start=0;
	queue_end=0;
	id=pos;
	sock=socky;
	finish=false;
	alive=true;
	//start a listener thread
	pthread_create(&thread, NULL, s_brthreadstart, this);
}

void Broadcaster::stop()
{
	logmsgf(LOG_DEBUG,"Broadcaster stop called");
	finish=true;
	logmsgf(LOG_DEBUG,"Lock");
	pthread_mutex_lock(&queue_mutex);
	/*
	logmsgf(LOG_DEBUG,"Signal");
	pthread_cond_signal(&queue_cv); //try this
	pthread_mutex_unlock(&queue_mutex);
	*/
	logmsgf(LOG_DEBUG,"Cancel");
	//pthread_cancel(thread); //finish the bastard
	
	logmsgf(LOG_DEBUG,"Join");
	//pthread_join(thread, NULL); //this can block the killer!
	alive=false;
	
	logmsgf(LOG_DEBUG,"UnLock");
	pthread_mutex_unlock(&queue_mutex);
	logmsgf(LOG_DEBUG,"Done");
}

void Broadcaster::threadstart()
{
	//printf("Broadcast thread\n");
	while (!finish)
	{
		//pop the packet to send
		//this must be done quickly
		unsigned int uid;
		int type;
		unsigned int datalen;
		if (finish)
			pthread_exit(NULL);
		//wait for an entry and lock the queue
		pthread_mutex_lock(&queue_mutex);
		while (queue_end==queue_start && !finish)
			pthread_cond_wait(&queue_cv, &queue_mutex);
		if (finish) 
		{
			pthread_mutex_unlock(&queue_mutex);
			logmsgf(LOG_DEBUG,"Appropriate finish");
			pthread_exit(NULL); //crash and burn
		}
		//pop stuff
		uid=queue[queue_start].uid;
		type=queue[queue_start].type;
		datalen=queue[queue_start].datalen;
		
		memcpy(send_data, queue[queue_start].data, datalen);
		queue_start++;
		
		if (queue_start==QUEUE_LENGTH)
			queue_start-=QUEUE_LENGTH;
		//unlock the queue
		pthread_mutex_unlock(&queue_mutex);
		if (finish)
			pthread_exit(NULL);
		//Send message
		if (Messaging::sendmessage(sock, type, uid, datalen, send_data))
		{
			sequencer->disconnect(id, "Broadcaster: Send error"); return;
		}
	}
	pthread_exit(NULL);
}

//this is called all the way from the receiver threads, we should process this swiftly
//and keep in mind that it is called crazily and concurently from lots of threads
//we MUST copy the data too
//also, this function can be called by threads owning clients_mutex !!!
void Broadcaster::queueMessage(unsigned int uid, int type, char* data, unsigned int len)
{
	//printf("Queue %i\n", type);
	//lock the queue
	pthread_mutex_lock(&queue_mutex);
	if (type!=MSG2_VEHICLE_DATA)
	{
		//this is an out of band message
		//queue at the start
		//push the new message
		queue_start--;
		if (queue_start<0) queue_start+=QUEUE_LENGTH;
		//the last message may disappear if the queue is full
		//note that we always keep a free slot to distinguish a full queue from an empty one
		if (queue_start==queue_end)
		{
			//pop the last message
			queue_end--;
			if (queue_end<0) queue_end+=QUEUE_LENGTH;
		}
		queue[queue_start].uid=uid;
		queue[queue_start].type=type;
		if (data) memcpy(queue[queue_start].data, data, len);
		queue[queue_start].datalen=len;
	}
	else 
	{
		//this is normal message
		//queue at the end
		int new_queue_end=queue_end+1;
		if (new_queue_end==QUEUE_LENGTH) new_queue_end-=QUEUE_LENGTH;
		if (new_queue_end!=queue_start)
		{
			queue[queue_end].uid=uid;
			queue[queue_end].type=type;
			if (data) memcpy(queue[queue_end].data, data, len);
			queue[queue_end].datalen=len;
			queue_end=new_queue_end;
		} //else the queue is full, we drop the packet
	} 
	//signal the thread
	pthread_cond_signal(&queue_cv);
	//unlock the queue
	pthread_mutex_unlock(&queue_mutex);
}

