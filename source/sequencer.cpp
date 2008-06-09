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
#include "sha1_util.h"
#include "listener.h"
#include "receiver.h"
#include "broadcaster.h"
#include "notifier.h"
#include "SocketW.h"
#include "logger.h"

#include <iostream>
#include <stdexcept>
#include <sstream>

//#define REFLECT_DEBUG 

void *s_klthreadstart(void* vid)
{
    STACKLOG;
	((Sequencer*)vid)->killerthreadstart();
	return NULL;
}

// init the singleton pointer
Sequencer* Sequencer::mInstance = NULL;

/// retreives the instance of the Sequencer
Sequencer* Sequencer::Instance() {
    STACKLOG;
	if(!mInstance) 
		mInstance = new Sequencer;
	return mInstance;
}


Sequencer::Sequencer() :  listener( NULL ), notifier( NULL ), clients( NULL ),
fuid( 1 ), freekillqueue( 0 ),  pwProtected(false), rconenabled( false ),
isSandbox(false), startTime ( Messaging::getTime() )
{
    STACKLOG;
}

Sequencer::~Sequencer()
{
    STACKLOG;
}

/**
 * Inililize, needs to be called before the class is used
 */ 
void Sequencer::initilize(char *pubip, int max_clients, char* servname,
char* terrname, int listenport, int smode, char *pass, char *rconpass,
bool _guimode)
{
    STACKLOG;
    
    Sequencer* instance  = Instance();
	instance->isSandbox  = !strcmp(terrname, "any");
	instance->servermode = smode;
	instance->maxclients = max_clients;
	instance->clients    = (client_t*)malloc(sizeof(client_t)*instance->maxclients);
	instance->listener   = new Listener(listenport);

	pthread_create(&instance->killerthread, NULL, s_klthreadstart, &instance);
	memset(instance->clients, 0, sizeof(client_t) * instance->maxclients);
	strncpy(instance->terrainName, terrname, 250);


	for (int i=0; i<instance->maxclients; i++) 
	{
		instance->clients[i].status      = FREE;
		instance->clients[i].sock        = NULL;
		instance->clients[i].broadcaster = new Broadcaster();
		instance->clients[i].receiver    = new Receiver();
	}
	if(pass && strnlen(pass, 250) > 0)
	{
		if(!SHA1FromString(instance->serverPassword, pass))
		{
			Logger::log(LOG_ERROR, "could not generate server SHA1 password hash!");
			exit(1);
		}
		Logger::log(LOG_DEBUG,"sha1(%s) = %s", pass, instance->serverPassword);
		instance->pwProtected = true;
	}

	if(rconpass && strnlen(rconpass, 250) > 0)
	{
		if(!SHA1FromString(instance->rconPassword, rconpass))
		{
			Logger::log(LOG_ERROR, "could not generate server SHA1 RCon password hash!");
			exit(1);
		}
		Logger::log(LOG_DEBUG,"sha1(%s) = %s", rconpass, instance->rconPassword);
		instance->rconenabled = true;
	}
	
	if(instance->servermode == SERVER_INET || instance->servermode == SERVER_AUTO)
		instance->notifier = new Notifier(
				pubip,
				listenport,
				instance->maxclients,
				servname,
				instance->terrainName,
		        instance->pwProtected,
		        instance->servermode,
		        instance->rconenabled);

#ifdef NCURSES
	instance->guimode = _guimode;
	if(instance->guimode)
	{
		initscr();
		// begin info window
		win_info = newwin(1, 80, 0, 0);

		wprintw(win_info, "Name: %s | ", servname);
		wprintw(win_info, "Port %d | ", listenport);
		wprintw(win_info, "Terrain: %s", terrname, max_clients);

		// now that we know max clients, we can actually make the box
		win_slots = newwin((max_clients+4), 44, 1, 0);
		win_log = newwin((max_clients+9), 80, (max_clients+5), 0);
		win_chat = newwin((max_clients+4), 35, 1, 45);
		scrollok(win_log, 1);
		scrollok(win_chat, 1);
		//box(win_slots, ACS_VLINE, ACS_HLINE);
		wrefresh(win_info);
		wrefresh(win_log);
		wrefresh(win_chat);
	}
#endif
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::cleanUp()
{
    STACKLOG;

    Sequencer* instance = Instance();
	for (int i=0; i<instance->maxclients && &instance->clients[i]; i++) 
	{
		Logger::log(LOG_DEBUG,"clients[%d]", i);
		if(instance->clients[i].broadcaster){
			Logger::log(LOG_DEBUG,"delete clients[%d].broadcaster", i);
			delete instance->clients[i].broadcaster;
		} else { 
			Logger::log(LOG_DEBUG,"clients[%d].broadcaster is null", i);
		}

		if(instance->clients[i].receiver) {
			Logger::log(LOG_DEBUG,"delete clients[%d].receiver", i); 
			delete instance->clients[i].receiver;
		} else { 
			Logger::log(LOG_DEBUG,"clients[%d].receiver is null", i);
		}
	}
	
	if( instance->notifier )
		delete instance->notifier;
	
	delete instance->listener;
	free(instance->clients);
	delete instance->mInstance;

#ifdef NCURSES
	endwin();
#endif
}

void Sequencer::notifyRoutine()
{
    STACKLOG;
	//we call the notify loop
    Sequencer* instance = Instance();
    instance->notifier->loop();
}

bool Sequencer::checkNickUnique(char *nick)
{
	// WARNING: be sure that this is only called within a clients_mutex lock!
	
	// check for duplicate names
	bool found = false;
	Sequencer* instance = Instance();
	for (int i = 0; i < instance->maxclients; i++)
	{
		if (!strcmp(nick, instance->clients[i].nickname)) 
		{
			found = true;
			break;
		}
	}
	return found;
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, user_credentials_t *user)
{
    STACKLOG;
    Sequencer* instance = Instance();
	//we have a confirmed client that wants to play
	//try to find a place for him
    instance->clients_mutex.lock();
	
	bool dupeNick = Sequencer::checkNickUnique(user->username);
	int dupecounter = 2;
	if(dupeNick)
	{
		char buf[20] = "";
		strncpy(buf, user->username, 20);
		Logger::log(LOG_WARN,"found duplicate nick, getting new one: %s\n", buf);
		if(strnlen(buf, 20) == 20)
			//shorten the string
			buf[18]=0;
		while(dupeNick)
		{
			sprintf(buf+strnlen(buf, 18), "%d", dupecounter++);
			Logger::log(LOG_DEBUG,"checked for duplicate nick (2): %s\n", buf);
			dupeNick = Sequencer::checkNickUnique(buf);
		}
		Logger::log(LOG_WARN,"chose alternate username: %s\n", buf);
		strncpy(user->username, buf, 20);
		
		// we should send him a message about the nickchange later...
	}
	
	// search a free slot
	int pos=-1;
	for (int i = 0; i < instance->maxclients; i++)
	{
		// an open spot if found, store it's position
		if (pos < 0 && FREE == instance->clients[i].status)
			pos = i;
	}
	
	if (pos < 0)
	{
		Logger::log(LOG_WARN,"join request from '%s' on full server: rejecting!", user->username);
		instance->clients_mutex.unlock();
		Messaging::sendmessage(sock, MSG2_FULL, 0, 0, 0);
		throw std::runtime_error("Server is full");
	}
	
	//okay, create the stuff
	instance->clients[pos].flow=false;
	instance->clients[pos].status=USED;
	instance->clients[pos].vehicle_name[0]=0;
	instance->clients[pos].position=Vector3(0,0,0);
	instance->clients[pos].rconretries=0;
	instance->clients[pos].rconauth=0;
	strncpy(instance->clients[pos].nickname, user->username, 20);
	strncpy(instance->clients[pos].uniqueid, user->uniqueid, 60);

	// replace bad characters
	for (unsigned int i=0; i<20; i++)
	{
		if(instance->clients[pos].nickname[i] == 0)
			break;
	}

	instance->clients[pos].uid=instance->fuid;
	instance->fuid++;
	instance->clients[pos].sock = sock;//this won't interlock
	instance->clients[pos].receiver->reset(instance->clients[pos].uid, sock);
	//this won't interlock
	instance->clients[pos].broadcaster->reset(instance->clients[pos].uid, sock,
			Sequencer::disconnect, Messaging::sendmessage); 
	instance->clients_mutex.unlock();

	Logger::log(LOG_VERBOSE,"Sequencer: New client created in slot %i", pos);
	printStats();
}

//this is called from the hearbeat notifier thread
int Sequencer::getHeartbeatData(char *challenge, char *hearbeatdata)
{
    STACKLOG;

    Sequencer* instance = Instance(); 
	SWBaseSocket::SWBaseError error;
	int clientnum = getNumClients();
	// lock this mutex after getNumClients is called to avoid a deadlock
	MutexLocker scoped_lock(instance->clients_mutex);

	sprintf(hearbeatdata, "%s\n" \
	                      "version2\n" \
	                      "%i\n", challenge, clientnum);
	if(clientnum > 0)
	{
		for (int i=0; i<instance->maxclients; i++)
		{
			if (instance->clients[i].status != FREE)
			{
				char playerdata[1024] = "";
				char positiondata[128] = "";
				instance->clients[i].position.toString(positiondata);
				sprintf(playerdata, "%d;%s;%s;%s;%s;%s\n", i, 
						instance->clients[i].vehicle_name, 
						instance->clients[i].nickname,
						positiondata, 
						instance->clients[i].sock->get_peerAddr(&error).c_str(),
						instance->clients[i].uniqueid);
				strcat(hearbeatdata, playerdata);
			}
		}
	}
	return 0;
}

int Sequencer::getNumClients()
{
    STACKLOG;
    Sequencer* instance = Instance(); 
	int count=0;
	MutexLocker scoped_lock(instance->clients_mutex);
	for( int i = 0; i < instance->maxclients; i++)
		if (instance->clients[i].status != FREE)
			count++;
	return count;
}

void Sequencer::killerthreadstart()
{
    STACKLOG;
    Sequencer* instance = Instance(); 
	Logger::log(LOG_DEBUG,"Killer thread ready");
	while (1)
	{
		SWBaseSocket::SWBaseError error;

		Logger::log(LOG_DEBUG,"Killer entering cycle");

		instance->killer_mutex.lock();
		while (instance->freekillqueue==0)
			instance->killer_mutex.wait(instance->killer_cv);
		//pop the kill queue
		int pos=instance->killqueue[0];
		instance->freekillqueue--;
		
		memcpy(instance->killqueue, &instance->killqueue[1], 
				sizeof(int) * instance->freekillqueue);
		instance->killer_mutex.unlock();

		Logger::log(LOG_DEBUG,"Killer called to kill %i", pos);
		instance->clients_mutex.lock(); //this won't interlock unless disconnect is called
		                    //from a clients_mutex owning thread
		Logger::log(LOG_DEBUG,"Killer got clients lock");
		if (instance->clients[pos].status==USED)
		{
			instance->clients[pos].status=BUSY;
			Logger::log(LOG_DEBUG,"Killer notifying");
			//notify the others
			for (int i=0; i<instance->maxclients; i++)
			{
				if (instance->clients[i].status == USED &&
						strlen(instance->clients[i].vehicle_name)>0)
				{
					instance->clients[i].broadcaster->queueMessage(
							instance->clients[pos].uid, MSG2_DELETE, 0, 0);
				}
			}
			Logger::log(LOG_DEBUG,"Killer done notifying");

			// CRITICAL ORDER OF EVENTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			// stop the broadcaster first, then disconnect the socket.
			// other wise there is a chance (being concurrent code) that the
			// socket will attempt to send a message between the disconnect
			// which makes the socket invalid) and the actual time of stoping
			// the bradcaster
			instance->clients[pos].broadcaster->stop();
			instance->clients[pos].sock->disconnect(&error);
			instance->clients[pos].receiver->stop();
			delete instance->clients[pos].sock;
			instance->clients[pos].sock = NULL;
			// END CRITICAL ORDER OF EVENTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			memset(instance->clients[pos].nickname, 0, 20);
			instance->clients[pos].status = FREE;
		}
		instance->clients_mutex.unlock();
		Logger::log(LOG_DEBUG,"Killer has properly killed %i", pos);
		printStats();
	}
}

void Sequencer::disconnect(int uid, char* errormsg)
{
    STACKLOG;
    Sequencer* instance = Instance(); 
    unsigned short pos = instance->getPosfromUid(uid);
	//this routine is a potential trouble maker as it can be called from many thread contexts
	//so we use a killer thread
	Logger::log(LOG_VERBOSE, "Disconnecting Slot %d: %s", pos, errormsg);
	MutexLocker scoped_lock(instance->killer_mutex);
	
	//first check if not already queued
	for (int i = 0; i < instance->freekillqueue; i++)	{
	    // check for duplicates
        if (instance->killqueue[i]==pos) {
            return;
        }
	}
	instance->killqueue[instance->freekillqueue]=pos;
	instance->freekillqueue++;
	instance->killer_cv.signal();
}

//this is called from the listener thread initial handshake
void Sequencer::enableFlow(int uid)
{
    STACKLOG;
    Sequencer* instance = Instance(); 
    unsigned short pos = instance->getPosfromUid(uid);
	MutexLocker scoped_lock(instance->clients_mutex);
	instance->clients[pos].flow=true;
}

//this is called from the listener thread initial handshake
void Sequencer::notifyAllVehicles(int uid)
{
    STACKLOG;
    Sequencer* instance = Instance(); 
    unsigned short pos = instance->getPosfromUid(uid);
	MutexLocker scoped_lock(instance->clients_mutex);
	for (int i=0; i<instance->maxclients; i++)
	{
		if (i!=pos && instance->clients[i].status == USED &&
				strlen(instance->clients[i].vehicle_name)>0)
		{
			char message[512];
			strcpy(message, instance->clients[i].vehicle_name);
			strcpy(message + strlen(instance->clients[i].vehicle_name) + 1,
					instance->clients[i].nickname);
			instance->clients[pos].broadcaster->queueMessage(
					instance->clients[i].uid, MSG2_USE_VEHICLE,
					message,
					(unsigned int)(
							strlen(instance->clients[i].vehicle_name) +
							strlen(instance->clients[i].nickname) ) + 2);
		}
		// not possible to have flow enabled but not have a truck... disconnect 
		if ( !strlen(instance->clients[i].vehicle_name) &&
				instance->clients[i].flow )
		{
			Logger::log(LOG_ERROR, "Client has flow enable but no truck name, "
					"disconnecting");
			disconnect(instance->clients[i].uid, "client appears to be disconnected");
		}
	}
}

void Sequencer::serverSay(std::string msg, int notto, int type)
{
    STACKLOG;
    Sequencer* instance = Instance(); 
	if(type==0)
		msg = std::string("^1 SERVER: ^9") + msg;
	//pthread_mutex_lock(&clients_mutex);
	for (int i = 0; i < instance->maxclients; i++)
		if (instance->clients[i].status == USED &&
				instance->clients[i].flow &&
				(notto==-1 || notto!=i))
			instance->clients[i].broadcaster->queueMessage(
					0, MSG2_CHAT,
					const_cast<char*>(msg.c_str()),
					(unsigned int)msg.size());
	//pthread_mutex_unlock(&clients_mutex);
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int uid, int type, char* data, unsigned int len)
{
	STACKLOG;
    Sequencer* instance = Instance(); 
    unsigned short pos = instance->getPosfromUid(uid);
	MutexLocker scoped_lock(instance->clients_mutex);
	bool publishData=false;
	if (type==MSG2_USE_VEHICLE) 
	{
		data[len]=0;
		strncpy(instance->clients[pos].vehicle_name, data, 129);
		Logger::log(LOG_VERBOSE,"On the fly vehicle registration for slot %d: %s",
		            pos, instance->clients[pos].vehicle_name);
		//printStats();
		//we alter the message to add user info
		strcpy(data + len + 1, instance->clients[pos].nickname);
		len += (int)strlen(instance->clients[pos].nickname) + 2;
		publishData = true;
	}
	
	else if (type==MSG2_DELETE)
	{
		Logger::log(LOG_INFO, "user %d disconnects on request", pos);
		disconnect(instance->clients[pos].uid, "disconnected on request");
	}
	else if (type==MSG2_RCON_COMMAND)
	{
		Logger::log(LOG_WARN, "user %d (%d) sends rcon command: %s",
				pos,
				instance->clients[pos].rconauth,
				data);
		if(instance->clients[pos].rconauth==1)
		{
			if(!strncmp(data, "kick", 4))
			{
				int player = -1;
				int res = sscanf(data, "kick %d", &player); 
				if(res == 1 && player != -1 && player < instance->maxclients)
				{
					if(instance->clients[player].status == FREE ||
							instance->clients[player].status == BUSY)
					{
						char *error = "cannot kick free or busy client";
						instance->clients[pos].broadcaster->queueMessage( 0,
							MSG2_RCON_COMMAND_FAILED, error, strlen(error) );
					} else if(instance->clients[player].status == USED) {
						Logger::log(LOG_WARN, "user %d kicked by user %d via rcmd",
								player, pos);
						char tmp[255]="";
						memset(tmp, 0, 255);
						sprintf(tmp, "player '%s' kicked successfully.",
								instance->clients[player].nickname);
						serverSay(std::string(tmp));

						instance->clients[pos].broadcaster->queueMessage( 0,
								MSG2_RCON_COMMAND_SUCCESS, tmp, strlen(tmp) );
						
						disconnect( instance->clients[player].uid, "kicked" );
					}
				} else {
					char *error = "invalid client number";
					instance->clients[pos].broadcaster->queueMessage( 0,
							MSG2_RCON_COMMAND_FAILED, error, strlen(error) );
				}
			}
		}
		else
			instance->clients[pos].broadcaster->queueMessage( 0,
					MSG2_RCON_COMMAND_FAILED, 0, 0 );
		
		publishData=false;
	}
	else if (type==MSG2_CHAT)
	{
		Logger::log(LOG_INFO, "CHAT| %s: %s", instance->clients[pos].nickname, data);
		publishData=true;
	}
	else if (type==MSG2_RCON_LOGIN)
	{
		if(instance->rconenabled && instance->clients[pos].rconretries < 3)
		{
			char pw[255]="";
			strncpy(pw, data, 255);
			pw[len]=0;
			Logger::log(LOG_DEBUG, "user %d  tries to log into RCON: server: "
					"%s, his: %s", pos, instance->rconPassword, pw);
			if(pw && strnlen(pw, 250) > 20 && !strcmp(instance->rconPassword, pw))
			{
				Logger::log(LOG_WARN, "user %d logged into RCON", pos);
				instance->clients[pos].rconauth=1;
				instance->clients[pos].broadcaster->queueMessage( 0,
						MSG2_RCON_LOGIN_SUCCESS, 0, 0 );
			}else
			{
				// pw incorrect or failed
				Logger::log(LOG_WARN, "user %d failed to login RCON, retry "
						"number %d", pos, instance->clients[pos].rconretries);
				instance->clients[pos].rconauth=0;
				instance->clients[pos].rconretries++;
				instance->clients[pos].broadcaster->queueMessage( 0,
						MSG2_RCON_LOGIN_FAILED, 0, 0 );
				Messaging::sendmessage( instance->clients[pos].sock,
						MSG2_RCON_LOGIN_FAILED, 0, 0, 0);
			}
		}else
		{
			Logger::log(LOG_WARN, "user %d failed to login RCON, as RCON is "
					"disabled %d", pos, instance->clients[pos].rconretries);
			instance->clients[pos].broadcaster->queueMessage( 0,
					MSG2_RCON_LOGIN_NOTAV, 0, 0 );
		}
		publishData=false;
	}
	else if (type==MSG2_VEHICLE_DATA)
	{
		float* fpt=(float*)(data+sizeof(oob_t));
		instance->clients[pos].position=Vector3(fpt[0], fpt[1], fpt[2]);
		publishData=true;
	}
	else if (type==MSG2_FORCE)
	{
		//this message is to be sent to only one destination
		unsigned int destuid=((netforce_t*)data)->target_uid;
		for (int i = 0; i < instance->maxclients; i++)
		{
			if (instance->clients[i].status == USED && 
				instance->clients[i].flow &&
				instance->clients[i].uid==destuid)
				instance->clients[i].broadcaster->queueMessage(
						instance->clients[pos].uid, type, data, len);
		}
		publishData=false;
	}
	
	if(publishData)
	{
		//just push to all the present clients
		for (int i = 0; i < instance->maxclients; i++)
		{
			if (instance->clients[i].status == USED &&
					instance->clients[i].flow &&i!=pos)
				instance->clients[i].broadcaster->queueMessage(
						instance->clients[pos].uid, type, data, len);
#ifdef REFLECT_DEBUG
			if (clients[i].status==USED && i==pos)
				clients[i].broadcaster->queueMessage(clients[pos].uid+100, type, data, len);
#endif
		}
	}
}

void Sequencer::printStats()
{
    STACKLOG;
    Sequencer* instance = Instance();
	MutexLocker scoped_lock(instance->clients_mutex);
	SWBaseSocket::SWBaseError error;
#ifdef NCURSES
	if(instance->guimode)
	{
		mvwprintw(win_slots, 1, 1, "Slot Status UID IP              Nickname");
		mvwprintw(win_slots, 2, 1, "------------------------------------------");
		for (int i=0; i<maxclients; i++)
		{
			//this is glitched: the rest of the line is not cleared !!!
			//I used to know what the problem was but I forgot it... I'll figure it out, eventually :)
			if (clients[i].status==FREE) 
				mvwprintw(win_slots, (i+3), 1, "%4i Free", i);
			else if (clients[i].status==BUSY)
				mvwprintw(win_slots, (i+3), 1, "%4i Busy %5i %-15s %.10s", i, clients[i].uid, "n/a", clients[i].nickname);
			else if (clients[i].status==USED)
				mvwprintw(win_slots, (i+3), 1, "%4i Used %5i %-15s %.10s", i, clients[i].uid, clients[i].sock->get_peerAddr(&error).c_str(), clients[i].nickname);
		}
		wrefresh(win_slots);
	} else
#endif
	{
		Logger::log(LOG_INFO, "Server occupancy:");

		Logger::log(LOG_INFO, "Slot Status   UID IP              Nickname, Vehicle");
		Logger::log(LOG_INFO, "--------------------------------------------------");
		for (int i = 0; i < instance->maxclients; i++)
		{
			if (instance->clients[i].status == FREE) 
				Logger::log(LOG_INFO, "%4i Free", i);
			else if (instance->clients[i].status == BUSY)
				Logger::log(LOG_INFO, "%4i Busy %5i %-16s %s, %s", i,
						instance->clients[i].uid, "-", 
						instance->clients[i].nickname, 
						instance->clients[i].vehicle_name);
			else 
				Logger::log(LOG_INFO, "%4i Used %5i %-16s %s, %s", i, 
						instance->clients[i].uid, 
						instance->clients[i].sock->get_peerAddr(&error).c_str(), 
						instance->clients[i].nickname, 
						instance->clients[i].vehicle_name);
		}
		Logger::log(LOG_INFO, "--------------------------------------------------");
		int timediff = Messaging::getTime() - instance->startTime;
		int uphours = timediff/60/60;
		int upminutes = (timediff-(uphours*60*60))/60;
		Logger::log(LOG_INFO, "- traffic statistics (uptime: %d hours, %d "
				"minutes):", uphours, upminutes);
		Logger::log(LOG_INFO, "- total: incoming: %0.2fMB , outgoing: %0.2fMB", 
				Messaging::getBandwitdthIncoming()/1024/1024, 
				Messaging::getBandwidthOutgoing()/1024/1024);
		Logger::log(LOG_INFO, "- rate (last minute): incoming: %0.1fkB/s , "
				"outgoing: %0.1fkB/s", 
				Messaging::getBandwitdthIncomingRate()/1024, 
				Messaging::getBandwidthOutgoingRate()/1024);
	}
}
// used to access the clients from the array rather than using the array pos it's self.
unsigned short Sequencer::getPosfromUid(const unsigned int& uid)
{
    STACKLOG;
    Sequencer* instance = Instance();
    
    for (unsigned short i = 0; i < instance->maxclients; i++)
    {
        if(instance->clients[i].uid == uid)
            return i;
    }
    
    // todo cleanup, this error would cause the program to crash unless caught 
    std::stringstream error_msg;
    error_msg << "could not find user id: " << uid;
    throw std::runtime_error(error_msg.str() );
}

char *Sequencer::getTerrainName()
{
	return Instance()->terrainName;
}

bool Sequencer::isPasswordProtected()
{
	return Instance()->pwProtected;
}

char* Sequencer::getServerPasswordHash()
{
	return Instance()->serverPassword; 
}
void Sequencer::unregisterServer()
{
	if( Instance()->notifier )
		Instance()->notifier->unregisterServer();
}


#ifdef NCURSES

bool Sequencer::getGUIMode()
{
	return Instance()->guimode;
}

WINDOW* Sequencer::getLogWindow()
{
	return Instance()->win_log;
}
#endif 

#if 0
// resets a client back to it's initial unused ready state.
unsigned int resetClient( const client_t& client)
{
	client.flow = false;
	if( client.broadcaster )
	{
		client.broadcaster->stop();
		delete client.broadcaster;
	}
	client.broadcaster = new Broadcaster();

	if( client.receiver )
	{
		client.receiver->stop();
		delete client.receiver;
	}
	client.receiver = new Broadcaster();
	
	if( client.sock )
	{
		client.sock->disconnect();
		delete client.sock;
	}
	client.sock = NULL;

}
#endif
