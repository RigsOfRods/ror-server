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
Sequencer& Sequencer::Instance() {
    STACKLOG;
	if(!mInstance) 
		mInstance = new Sequencer;
	return (*mInstance);
}

Sequencer::Sequencer() : pwProtected(false), isSandbox(false)
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
void Sequencer::initilize(char *pubip, int max_clients, char* servname, char* terrname, int listenport, int smode, char *pass, char *rconpass, bool _guimode)
{
    STACKLOG;
	guimode = _guimode;
	strncpy(terrainName, terrname, 250);
	isSandbox = !strcmp(terrname, "any");

	startTime = Messaging::getTime();
	freekillqueue=0;
	servermode=smode;
	pthread_create(&killerthread, NULL, s_klthreadstart, this);
	
	fuid = 1;
	notifier = NULL;
	maxclients=max_clients;
	clients = (client_t*)malloc(sizeof(client_t)*maxclients);
	    
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
		if(!SHA1FromString(serverPassword, pass))
		{
			Logger::log(LOG_ERROR, "could not generate server SHA1 password hash!");
			exit(1);
		}
		Logger::log(LOG_DEBUG,"sha1(%s) = %s", pass, serverPassword);
		pwProtected = true;
	}

	rconenabled = false;
	if(rconpass && strnlen(rconpass, 250)>0)
	{
		if(!SHA1FromString(rconPassword, rconpass))
		{
			Logger::log(LOG_ERROR, "could not generate server SHA1 RCon password hash!");
			exit(1);
		}
		Logger::log(LOG_DEBUG,"sha1(%s) = %s", rconpass, rconPassword);
		rconenabled = true;
	}

	if(guimode)
	{
#ifdef NCURSES
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
#endif
	}

	if(servermode == SERVER_INET || servermode == SERVER_AUTO)
		notifier = new Notifier(pubip, listenport, max_clients, servname,
		        terrname, pwProtected, servermode, rconenabled);
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::cleanUp()
{
    STACKLOG;
	if(notifier) delete notifier;
	for (int i=0; i<maxclients && &clients[i]; i++) 
	{
		Logger::log(LOG_DEBUG,"clients[%d]", i);
		if(clients[i].broadcaster){
			Logger::log(LOG_DEBUG,"delete clients[%d].broadcaster", i);
			delete clients[i].broadcaster;
		} else { 
			Logger::log(LOG_DEBUG,"clients[%d].broadcaster is null", i);
		}

		if(clients[i].receiver) {
			Logger::log(LOG_DEBUG,"delete clients[%d].receiver", i); 
			delete clients[i].receiver;
		} else { 
			Logger::log(LOG_DEBUG,"clients[%d].receiver is null", i);
		}
	}
	delete listener;
	free(clients);

#ifdef NCURSES
	endwin();
#endif
}

void Sequencer::notifyRoutine()
{
    STACKLOG;
	//we call the notify loop
	notifier->loop();
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, user_credentials_t *user)
{
    STACKLOG;
	//we have a confirmed client that wants to play
	//try to find a place for him
	clients_mutex.lock();
	
	int pos=-1;
	for (int i = 0; i < maxclients; i++)
	{
	    // validate the requested nick against the slot that is being scanned
	    // for openings.
		if (!strcmp(user->username, clients[i].nickname)) 
		{
			clients_mutex.unlock();
			Logger::log(LOG_WARN,"Dupe nick found: '%s' rejecting!", user->username); //a dupe, kill it!
			char *msg = "Duplicate name, please choose another one!";
			//lack of proper protocol msg
			Messaging::sendmessage(sock, MSG2_BANNED, 0, strlen(msg), msg);
			throw std::runtime_error("duplicate nick found");
		}
		// an open spot if found, store it's position
		if (pos < 0 && FREE == clients[i].status)
			pos = i;
	}
	
	if (pos < 0)
	{
		Logger::log(LOG_WARN,"join request from '%s' on full server: rejecting!", user->username);
		clients_mutex.unlock();
		Messaging::sendmessage(sock, MSG2_FULL, 0, 0, 0);
		throw std::runtime_error("Server is full");
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
	}

	clients[pos].uid=fuid;
	fuid++;
	clients[pos].sock=sock;
	clients[pos].receiver->reset(clients[pos].uid, sock); //this won't interlock
	clients[pos].broadcaster->reset(clients[pos].uid, sock); //this won't interlock
	clients_mutex.unlock();

	Logger::log(LOG_VERBOSE,"Sequencer: New client created in slot %i", pos);
	printStats();
}

//this is called from the hearbeat notifier thread
int Sequencer::getHeartbeatData(char *challenge, char *hearbeatdata)
{
    STACKLOG;
	SWBaseSocket::SWBaseError error;
	int clientnum = getNumClients();
	// lock this mutex after getNumClients is called to avoid a deadlock
	MutexLocker scoped_lock(clients_mutex);

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
	return 0;
}

int Sequencer::getNumClients()
{
    STACKLOG;
	int count=0;
	MutexLocker scoped_lock(clients_mutex);
	for (int i=0; i<maxclients; i++) if (clients[i].status!=FREE) count++;
	return count;
}

void Sequencer::killerthreadstart()
{
    STACKLOG;
	Logger::log(LOG_DEBUG,"Killer thread ready");
	while (1)
	{
		SWBaseSocket::SWBaseError error;

		Logger::log(LOG_DEBUG,"Killer entering cycle");

		killer_mutex.lock();
		while (freekillqueue==0)
			killer_mutex.wait(killer_cv);
		//pop the kill queue
		int pos=killqueue[0];
		freekillqueue--;
		
		memcpy(killqueue, &killqueue[1], sizeof(int)*freekillqueue);
		killer_mutex.unlock();

		Logger::log(LOG_DEBUG,"Killer called to kill %i", pos);
		clients_mutex.lock(); //this won't interlock unless disconnect is called
		                    //from a clients_mutex owning thread
		Logger::log(LOG_DEBUG,"Killer got clients lock");
		if (clients[pos].status==USED)
		{
			clients[pos].status=BUSY;
			Logger::log(LOG_DEBUG,"Killer notifying");
			//notify the others
			for (int i=0; i<maxclients; i++)
			{
				if (clients[i].status==USED && strlen(clients[i].vehicle_name)>0)
				{
					clients[i].broadcaster->queueMessage(clients[pos].uid, MSG2_DELETE, 0, 0);
				}
			}
			Logger::log(LOG_DEBUG,"Killer done notifying");

			// things are less critical, we can take time to cleanup properly and
			// synchronously
			clients[pos].sock->disconnect(&error);
			clients[pos].receiver->stop();
			clients[pos].broadcaster->stop();
			memset(clients[pos].nickname, 0, 20);
			delete clients[pos].sock;
			clients[pos].sock = NULL;
			clients[pos].status = FREE;
		}
		clients_mutex.unlock();
		Logger::log(LOG_DEBUG,"Killer has properly killed %i", pos);
		printStats();
	}
}

void Sequencer::disconnect(int uid, char* errormsg)
{
    STACKLOG;
    unsigned short pos = getPosfromUid(uid);
	//this routine is a potential trouble maker as it can be called from many thread contexts
	//so we use a killer thread
	Logger::log(LOG_VERBOSE, "Disconnecting Slot %d: %s", pos, errormsg);
	MutexLocker scoped_lock(killer_mutex);
	
	//first check if not already queued
	bool found=false;
	for (int i=0; i<freekillqueue; i++)
	    // check for duplicates
        if (killqueue[i]==pos)
    	{
            found=true;
            break;
        };
        
	if (!found)
	{
		killqueue[freekillqueue]=pos;
		freekillqueue++;
		killer_cv.signal();
	}
}

//this is called from the listener thread initial handshake
void Sequencer::enableFlow(int uid)
{
    STACKLOG;
    unsigned short pos = getPosfromUid(uid);
	MutexLocker scoped_lock(clients_mutex);
	clients[pos].flow=true;
}

//this is called from the listener thread initial handshake
void Sequencer::notifyAllVehicles(int uid)
{
    STACKLOG;
    unsigned short pos = getPosfromUid(uid);
	MutexLocker scoped_lock(clients_mutex);
	for (int i=0; i<maxclients; i++)
	{
		if (i!=pos && clients[i].status==USED && strlen(clients[i].vehicle_name)>0)
		{
			char message[512];
			strcpy(message, clients[i].vehicle_name);
			strcpy(message+strlen(clients[i].vehicle_name)+1, clients[i].nickname);
			clients[pos].broadcaster->queueMessage(clients[i].uid, MSG2_USE_VEHICLE,
					message, (unsigned int)(strlen(clients[i].vehicle_name)+strlen(clients[i].nickname))+2);
		}
		// not possible to have flow enabled but not have a truck... disconnect 
		if ( !strlen(clients[i].vehicle_name) && clients[i].flow )
		{
			Logger::log(LOG_ERROR, "Client has flow enable but no truck name, disconnecting");
			disconnect(clients[i].uid, "client appears to be disconnected");
		}
	}
}

void Sequencer::serverSay(std::string msg, int notto, int type)
{
    STACKLOG;
	if(type==0)
		msg = std::string("^1 SERVER: ^9") + msg;
	//pthread_mutex_lock(&clients_mutex);
	for (int i=0; i<maxclients; i++)
		if (clients[i].status==USED && clients[i].flow && (notto==-1 || notto!=i))
			clients[i].broadcaster->queueMessage(0, MSG2_CHAT, const_cast<char*>(msg.c_str()), (unsigned int)msg.size());
	//pthread_mutex_unlock(&clients_mutex);
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int uid, int type, char* data, unsigned int len)
{
    unsigned short pos = getPosfromUid(uid);
	MutexLocker scoped_lock(clients_mutex);
	bool publishData=false;
	if (type==MSG2_USE_VEHICLE) 
	{
		data[len]=0;
		strncpy(clients[pos].vehicle_name, data, 129);
		Logger::log(LOG_VERBOSE,"On the fly vehicle registration for slot %d: %s",
		            pos, clients[pos].vehicle_name);
		//printStats();
		//we alter the message to add user info
		strcpy(data+len+1, clients[pos].nickname);
		len+=(int)strlen(clients[pos].nickname)+2;
		publishData=true;
	}
	
	else if (type==MSG2_DELETE)
	{
		Logger::log(LOG_INFO, "user %d disconnects on request", pos);
		disconnect(clients[pos].uid, "disconnected on request");
	}
	else if (type==MSG2_RCON_COMMAND)
	{
		Logger::log(LOG_WARN, "user %d (%d) sends rcon command: %s", pos, clients[pos].rconauth, data);
		if(clients[pos].rconauth==1)
		{
			if(!strncmp(data, "kick", 4))
			{
				int player = -1;
				int res = sscanf(data, "kick %d", &player); 
				if(res == 1 && player != -1 && player < maxclients)
				{
					if(clients[player].status == FREE ||
					        clients[player].status == BUSY)
					{
						char *error = "cannot kick free or busy client";
						if( Messaging::sendmessage(clients[pos].sock,
								MSG2_RCON_COMMAND_FAILED, 0, strlen(error),
								error) )
						{
							disconnect( pos, "error sending message to client");
						}
					} else if(clients[player].status == USED)
					{
						Logger::log(LOG_WARN, "user %d kicked by user %d via rcmd",
								player, pos);
						char tmp[255]="";
						memset(tmp, 0, 255);
						sprintf(tmp, "player '%s' kicked successfully.", clients[player].nickname);
						serverSay(std::string(tmp));
						if( Messaging::sendmessage(clients[pos].sock,
						        MSG2_RCON_COMMAND_SUCCESS, 0, strlen(tmp), tmp) )
						{
							disconnect( pos, "error sending message to client");
						}
						
						disconnect(clients[player].uid, "kicked");
					}
				} else 
				{
					char *error = "invalid client number";
					if( Messaging::sendmessage(clients[pos].sock,
							MSG2_RCON_COMMAND_FAILED, 0, strlen(error), error) )
					{
						disconnect( pos, "error sending message to client");
					}
				}
			}
		}
		else
			Messaging::sendmessage(clients[pos].sock, MSG2_RCON_COMMAND_FAILED, 0, 0, 0);
		publishData=false;
	}
	else if (type==MSG2_CHAT)
	{
		Logger::log(LOG_INFO, "CHAT| %s: %s", clients[pos].nickname, data);
		publishData=true;
	}
	else if (type==MSG2_RCON_LOGIN)
	{
		if(rconenabled && clients[pos].rconretries < 3)
		{
			char pw[255]="";
			strncpy(pw, data, 255);
			pw[len]=0;
			Logger::log(LOG_DEBUG, "user %d  tries to log into RCON: server: %s, his: %s", pos, rconPassword, pw);
			if(pw && strnlen(pw, 250) > 20 && !strcmp(rconPassword, pw))
			{
				Logger::log(LOG_WARN, "user %d logged into RCON", pos);
				clients[pos].rconauth=1;
				Messaging::sendmessage(clients[pos].sock, MSG2_RCON_LOGIN_SUCCESS, 0, 0, 0);
			}else
			{
				// pw incorrect or failed
				Logger::log(LOG_WARN, "user %d failed to login RCON, retry number %d", pos, clients[pos].rconretries);
				clients[pos].rconauth=0;
				clients[pos].rconretries++;
				Messaging::sendmessage(clients[pos].sock, MSG2_RCON_LOGIN_FAILED, 0, 0, 0);
			}
		}else
		{
			Logger::log(LOG_WARN, "user %d failed to login RCON, as RCON is disabled %d", pos, clients[pos].rconretries);
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
}

void Sequencer::printStats()
{
    STACKLOG;
	MutexLocker scoped_lock(clients_mutex);
	SWBaseSocket::SWBaseError error;
	if(guimode)
	{
#ifdef NCURSES
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
#endif
	} else
	{
		Logger::log(LOG_INFO, "Server occupancy:");

		Logger::log(LOG_INFO, "Slot Status   UID IP              Nickname, Vehicle");
		Logger::log(LOG_INFO, "--------------------------------------------------");
		for (int i=0; i<maxclients; i++)
		{
			if (clients[i].status==FREE) 
				Logger::log(LOG_INFO, "%4i Free", i);
			else if (clients[i].status==BUSY)
				Logger::log(LOG_INFO, "%4i Busy %5i %-16s %s, %s", i, clients[i].uid, "-", clients[i].nickname, clients[i].vehicle_name);
			else 
				Logger::log(LOG_INFO, "%4i Used %5i %-16s %s, %s", i, clients[i].uid, clients[i].sock->get_peerAddr(&error).c_str(), clients[i].nickname, clients[i].vehicle_name);
		}
		Logger::log(LOG_INFO, "--------------------------------------------------");
		int timediff = Messaging::getTime()-startTime;
		int uphours = timediff/60/60;
		int upminutes = (timediff-(uphours*60*60))/60;
		Logger::log(LOG_INFO, "- traffic statistics (uptime: %d hours, %d minutes):", uphours, upminutes);
		Logger::log(LOG_INFO, "- total: incoming: %0.2fMB , outgoing: %0.2fMB", Messaging::getBandwitdthIncoming()/1024/1024, Messaging::getBandwidthOutgoing()/1024/1024);
		Logger::log(LOG_INFO, "- rate (last minute): incoming: %0.1fkB/s , outgoing: %0.1fkB/s", Messaging::getBandwitdthIncomingRate()/1024, Messaging::getBandwidthOutgoingRate()/1024);
	}
}
// used to access the clients from the array rather than using the array pos it's self.
unsigned short Sequencer::getPosfromUid(const unsigned int& uid)
{
    STACKLOG;
    for (unsigned short i = 0; i < maxclients; i++)
    {
        if(clients[i].uid == uid)
            return i;
    }
    std::stringstream error_msg;
    error_msg << "could not find user id: " << uid;
    throw std::runtime_error(error_msg.str() );
}

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
