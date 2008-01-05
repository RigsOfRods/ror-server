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
#include "sequencer.h"
#include "messaging.h"
#include "rornet.h"
#include "sha1_util.h"

//#define REFLECT_DEBUG 

void *s_klthreadstart(void* vid)
{
	((Sequencer*)vid)->killerthreadstart();
	return NULL;
}

// init the singleton pointer
Sequencer* Sequencer::TheInstance = NULL;

/// retreives the instance of the Sequencer
Sequencer& Sequencer::Instance() {
	if(!TheInstance) 
		TheInstance = new Sequencer;
	return (*TheInstance);
}

Sequencer::Sequencer() : pwProtected(false), isSandbox(false)
{
}

Sequencer::~Sequencer()
{
}

/**
 * Inililize, needs to be called before the class is used
 */ 
void Sequencer::initilize(char *pubip, int max_clients, char* servname, char* terrname, int listenport, int smode, char *pass, char *rconpass)
{
	pthread_mutex_init(&killer_mutex, NULL);
	pthread_cond_init(&killer_cv, NULL);
	pthread_mutex_init(&clients_mutex, NULL);

	strncpy(terrainName, terrname, 250);
	isSandbox = !strcmp(terrname, "any");

	freekillqueue=0;
	servermode=smode;
	pthread_create(&killerthread, NULL, s_klthreadstart, this);
	fuid=1;
	notifier=0;
	maxclients=max_clients;
	clients=(client_t*)malloc(sizeof(client_t)*maxclients);
	memset(clients, 0, sizeof(client_t)*maxclients);

	for (int i=0; i<maxclients; i++) 
	{
		clients[i].status=FREE;
		clients[i].broadcaster=new Broadcaster();
		clients[i].receiver=new Receiver();
	}

	listener=new Listener(listenport);

	if(pass && strnlen(pass, 250)>0)
	{
		char buffer[40];
		memset(buffer, 0, 40);
		strncpy(buffer, pass, 40);
		buffer[40]=0;
		
		char result[40];
		memset(result, 0, 40);
		if(!SHA1FromString(result, buffer))
		{
			logmsgf(LOG_ERROR, "could not generate server SHA1 password hash!");
			exit(1);
		}
		strncpy(serverPassword, result, 40);
		pwProtected = true;
	}

	rconenabled = false;
	if(rconpass && strnlen(rconpass, 250)>0)
	{
		char buffer[40];
		memset(buffer, 0, 40);
		strncpy(buffer, rconpass, 40);
		
		char result[255];
		memset(result, 0, 255);
		if(!SHA1FromString(result, buffer))
		{
			logmsgf(LOG_ERROR, "could not generate server SHA1 RCon password hash!");
			exit(1);
		}
		strncpy(rconPassword, result, 40);
		logmsgf(LOG_DEBUG,"sha1(%s) = %s", rconpass, rconPassword);
		rconenabled = true;
	}

	if(servermode == SERVER_INET || servermode == SERVER_AUTO)
		notifier=new Notifier(pubip, listenport, max_clients, servname, terrname, pwProtected, servermode, rconenabled);
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::cleanUp()
{
	for (int i=0; i<maxclients && &clients[i]; i++) 
	{
		logmsgf(LOG_DEBUG,"clients[%d]", i);
		if(clients[i].broadcaster){
			logmsgf(LOG_DEBUG,"delete clients[%d].broadcaster", i);
			delete clients[i].broadcaster;
		} else { 
			logmsgf(LOG_DEBUG,"clients[%d].broadcaster is null", i);
		}

		if(clients[i].receiver) {
			logmsgf(LOG_DEBUG,"delete clients[%d].receiver", i); 
			delete clients[i].receiver;
		} else { 
			logmsgf(LOG_DEBUG,"clients[%d].receiver is null", i);
		}
	}
	if(notifier) delete notifier;
	delete listener;
	free(clients);
}

void Sequencer::notifyRoutine()
{
	//we call the notify loop
	notifier->loop();
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, user_credentials_t *user)
{
	//we have a confirmed client that wants to play
	//try to find a place for him
	pthread_mutex_lock(&clients_mutex);
	int pos=0;
	for (pos=0; pos<maxclients; pos++)
	{
		if (!strcmp(user->username, clients[pos].nickname)) //validate the requested nick against the slot that
		{
			// is being scanned for openings.
			logmsgf(LOG_DEBUG,"Dupe nick found: '%s' rejecting!", user->username); //a dupe, kill it!
			pthread_mutex_unlock(&clients_mutex);
			char *msg = "Duplicate name, please choose another one!";
			Messaging::sendmessage(sock, MSG2_BANNED, 0, (unsigned int)strlen(msg), msg); //lack of proper protocol msg
			return;
		} else
		{
			//Now that we've checked against dupe nicks, we can see if the pos is free
			if (clients[pos].status==FREE) 
				break; 
		} 
	}

	if (pos==maxclients)
	{
		logmsgf(LOG_WARN,"join request from '%s' on full server: rejecting!", user->username);
		pthread_mutex_unlock(&clients_mutex);
		Messaging::sendmessage(sock, MSG2_FULL, 0, 0, 0);
		return;
	}
	
	//okay, create the stuff
	clients[pos].flow=false;
	clients[pos].status=USED;
	clients[pos].vehicle_name[0]=0;
	clients[pos].position=Vector3(0,0,0);
	clients[pos].rconretries=0;
	clients[pos].rconauth=0;
	strncpy(clients[pos].nickname, user->username, 20);
	strncpy(clients[pos].uniqueid, user->uniqueid, 60);

	// replace bad characters
	for (unsigned int i=0; i<20; i++)
	{
		if(clients[pos].nickname[i] == 0)
			break;
		if (clients[pos].nickname[i]<32 || clients[pos].nickname[i]>127 || clients[pos].nickname[i]==';') 
			clients[pos].nickname[i]='#';
	}

	clients[pos].uid=fuid;
	fuid++;
	clients[pos].sock=sock;
	clients[pos].receiver->reset(pos, sock); //this won't interlock
	clients[pos].broadcaster->reset(pos, sock); //this won't interlock
	pthread_mutex_unlock(&clients_mutex);
	Messaging::sendmessage(sock, MSG2_WELCOME, clients[pos].uid, 0, 0);
	logmsgf(LOG_DEBUG,"Sequencer: New client created in slot %i", pos);
	printStats();
}

//this is called from the hearbeat notifier thread
int Sequencer::getHeartbeatData(char *challenge, char *hearbeatdata)
{
	SWBaseSocket::SWBaseError error;
	pthread_mutex_lock(&clients_mutex);
	int clientnum =0;
	for (int i=0; i<maxclients; i++) if (clients[i].status!=FREE) clientnum++;

	sprintf(hearbeatdata, "%s\n" \
	                      "version2\n" \
	                      "%i\n", challenge, clientnum);
	if(clientnum > 0)
	{
		for (int i=0; i<maxclients; i++)
		{
			if (clients[i].status != FREE)
			{
				char playerdata[1024] = "";
				char positiondata[128] = "";
				clients[i].position.toString(positiondata);
				sprintf(playerdata, "%d;%s;%s;%s;%s;%s\n", i, clients[i].vehicle_name, clients[i].nickname, positiondata, clients[i].sock->get_peerAddr(&error).c_str(), clients[i].uniqueid);
				strcat(hearbeatdata, playerdata);
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
	return 0;
}

int Sequencer::getNumClients()
{
	int count=0;
	pthread_mutex_lock(&clients_mutex);
	for (int i=0; i<maxclients; i++) if (clients[i].status!=FREE) count++;
	pthread_mutex_unlock(&clients_mutex);
	return count;
}

void Sequencer::killerthreadstart()
{
	logmsgf(LOG_DEBUG,"Killer thread ready");
	while (1)
	{
		SWBaseSocket::SWBaseError error;

		logmsgf(LOG_DEBUG,"Killer entering cycle");

		pthread_mutex_lock(&killer_mutex);
		while (freekillqueue==0)
			pthread_cond_wait(&killer_cv, &killer_mutex);
		//pop the kill queue
		int pos=killqueue[0];
		freekillqueue--;
		memcpy(killqueue, &killqueue[1], sizeof(int)*freekillqueue);
		pthread_mutex_unlock(&killer_mutex);

		logmsgf(LOG_DEBUG,"Killer called to kill %i", pos);
		pthread_mutex_lock(&clients_mutex); //this won't interlock unless
											//disconnect is called from a clients_mutex owning thread
		logmsgf(LOG_DEBUG,"Killer got clients lock");
		if (clients[pos].status==USED)
		{
			clients[pos].status=BUSY;
			logmsgf(LOG_DEBUG,"Killer notifying");
			//notify the others
			for (int i=0; i<maxclients; i++)
			{
				if (clients[i].status==USED && strlen(clients[i].vehicle_name)>0)
				{
					clients[i].broadcaster->queueMessage(clients[pos].uid, MSG2_DELETE, 0, 0);
				}
			}
			logmsgf(LOG_DEBUG,"Killer done notifying");
			//we release the mutex now
			pthread_mutex_unlock(&clients_mutex);

			//things are less critical, we can take time to cleanup properly and synchronously
			logmsgf(LOG_DEBUG,"Killer force disconnect");
			clients[pos].sock->disconnect(&error);
			logmsgf(LOG_DEBUG,"Killer stopping receiver");
			clients[pos].receiver->stop();
			logmsgf(LOG_DEBUG,"Killer stopping broadcaster");
			clients[pos].broadcaster->stop();
			memset(clients[pos].nickname, 0, 20); // Clear the nick data or else nicks will only be able to be used once!
			logmsgf(LOG_DEBUG,"Killer deleting socket");
			delete clients[pos].sock;
			pthread_mutex_lock(&clients_mutex); 
			logmsgf(LOG_DEBUG,"Killer got second clients lock");
			clients[pos].status=FREE;
		}
		pthread_mutex_unlock(&clients_mutex);
		logmsgf(LOG_DEBUG,"Killer has properly killed %i", pos);
	}
}

void Sequencer::disconnect(int pos, char* errormsg)
{
	//this routine is a potential trouble maker as it can be called from many thread contexts
	//so we use a killer thread
	logmsgf(LOG_DEBUG,"Disconnecting Slot %d: %s", pos, errormsg);
	pthread_mutex_lock(&killer_mutex);
	//first check if not already queued
	bool found=false;
	for (int i=0; i<freekillqueue; i++) if (killqueue[i]==pos) {found=true; break;};
	if (!found)
	{
		killqueue[freekillqueue]=pos;
		freekillqueue++;
		pthread_cond_signal(&killer_cv);
	}
	pthread_mutex_unlock(&killer_mutex);
}

//this is called from the listener thread initial handshake
void Sequencer::enableFlow(int pos)
{
	pthread_mutex_lock(&clients_mutex);
	clients[pos].flow=true;
	pthread_mutex_unlock(&clients_mutex);
}

//this is called from the listener thread initial handshake
void Sequencer::notifyAllVehicles(int pos)
{
	pthread_mutex_lock(&clients_mutex);
	for (int i=0; i<maxclients; i++)
	{
		if (i!=pos && clients[i].status==USED && strlen(clients[i].vehicle_name)>0)
		{
			char message[512];
			strcpy(message, clients[i].vehicle_name);
			strcpy(message+strlen(clients[i].vehicle_name)+1, clients[i].nickname);
			clients[pos].broadcaster->queueMessage(clients[i].uid, MSG2_USE_VEHICLE, message, (int)(strlen(clients[i].vehicle_name)+strlen(clients[i].nickname))+2);
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int pos, int type, char* data, unsigned int len)
{
	pthread_mutex_lock(&clients_mutex);
	bool publishData=false;
	if (type==MSG2_USE_VEHICLE) 
	{
		data[len]=0;
		strncpy(clients[pos].vehicle_name, data, 129);
		logmsgf(LOG_DEBUG,"On the fly vehicle registration for slot %d: %s", pos, clients[pos].vehicle_name);
		//printStats();
		//we alter the message to add user info
		strcpy(data+len+1, clients[pos].nickname);
		len+=(int)strlen(clients[pos].nickname)+2;
		publishData=true;
	}
	
	else if (type==MSG2_DELETE)
	{
		disconnect(pos, "disconnected on request");
	}
	else if (type==MSG2_RCON_COMMAND)
	{
		// XXX: TODO: handle rcon command stuff here
		logmsgf(LOG_DEBUG, "user %d (%d) sends rcon command: %s", pos, clients[pos].rconauth, data);
		if(clients[pos].rconauth==1)
			Messaging::sendmessage(clients[pos].sock, MSG2_RCON_COMMAND_SUCCESS, 0, 0, 0);
		else
			Messaging::sendmessage(clients[pos].sock, MSG2_RCON_COMMAND_FAILED, 0, 0, 0);
		publishData=false;
	}
	else if (type==MSG2_CHAT)
	{
		logmsgf(LOG_WARN, "CHAT| %s: %s", clients[pos].nickname, data);
		publishData=true;
	}
	else if (type==MSG2_RCON_LOGIN)
	{
		if(rconenabled && clients[pos].rconretries < 3)
		{
			char pw[255]="";
			strncpy(pw, data, 255);
			pw[len]=0;
			logmsgf(LOG_DEBUG, "user %d  tries to loginto RCON: server: %s, his: %s", pos, rconPassword, pw);
			if(pw && strnlen(pw, 250) > 20 && !strcmp(rconPassword, pw))
			{
				logmsgf(LOG_WARN, "user %d logged into RCON", pos);
				clients[pos].rconauth=1;
				Messaging::sendmessage(clients[pos].sock, MSG2_RCON_LOGIN_SUCCESS, 0, 0, 0);
			}else
			{
				// pw incorrect or failed
				logmsgf(LOG_WARN, "user %d failed to login RCON, retry number %d", pos, clients[pos].rconretries);
				clients[pos].rconauth=0;
				clients[pos].rconretries++;
				Messaging::sendmessage(clients[pos].sock, MSG2_RCON_LOGIN_FAILED, 0, 0, 0);
			}
		}else
		{
			logmsgf(LOG_WARN, "user %d failed to login RCON, as RCON is disabled %d", pos, clients[pos].rconretries);
			Messaging::sendmessage(clients[pos].sock, MSG2_RCON_LOGIN_NOTAV, 0, 0, 0);
		}
		publishData=false;
	}
	else if (type==MSG2_VEHICLE_DATA)
	{
		float* fpt=(float*)(data+sizeof(oob_t));
		clients[pos].position=Vector3(fpt[0], fpt[1], fpt[2]);
		publishData=true;
	}
	else if (type==MSG2_FORCE)
	{
		//this message is to be sent to only one destination
		unsigned int destuid=((netforce_t*)data)->target_uid;
		for (int i=0; i<maxclients; i++)
		{
			if (clients[i].status==USED && clients[i].flow && clients[i].uid==destuid)
				clients[i].broadcaster->queueMessage(clients[pos].uid, type, data, len);
		}
		publishData=false;
	}
	
	if(publishData)
	{
		//just push to all the present clients
		for (int i=0; i<maxclients; i++)
		{
			if (clients[i].status==USED && clients[i].flow && i!=pos)
				clients[i].broadcaster->queueMessage(clients[pos].uid, type, data, len);
#ifdef REFLECT_DEBUG
			if (clients[i].status==USED && i==pos)
				clients[i].broadcaster->queueMessage(clients[pos].uid+100, type, data, len);
#endif
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void Sequencer::printStats()
{
	SWBaseSocket::SWBaseError error;
	printf("Server occupancy:");
	if(isSandbox)
		printf(" (Sandbox mode!)");
	printf("\n");

	printf("Slot Status   UID IP              Nickname, Vehicle\n");
	printf("--------------------------------------------------\n");
	pthread_mutex_lock(&clients_mutex);
	for (int i=0; i<maxclients; i++)
	{
		printf("%4i", i);
		if (clients[i].status==FREE) printf(" Free\n");
		else if (clients[i].status==BUSY)
			printf(" Busy %5i %-16s %s, %s\n", clients[i].uid, "-", clients[i].nickname, clients[i].vehicle_name);
		else 
			printf(" Used %5i %-16s %s, %s\n", clients[i].uid, clients[i].sock->get_peerAddr(&error).c_str(), clients[i].nickname, clients[i].vehicle_name);
	}
	printf("--------------------------------------------------\n");
	printf("- traffic statistics:\n");
	printf("- total: incoming: %0.1fkB , outgoing: %0.1fkB\n", Messaging::getBandwitdthIncoming()/1024, Messaging::getBandwidthOutgoing()/1024);
	printf("- rate (last minute): incoming: %0.1fkB/s , outgoing: %0.1fkB/s\n", Messaging::getBandwitdthIncomingRate()/1024, Messaging::getBandwidthOutgoingRate()/1024);
	pthread_mutex_unlock(&clients_mutex);
}


