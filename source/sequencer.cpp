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
#include "userauth.h"
#include "SocketW.h"
#include "logger.h"
#include "config.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
//#define REFLECT_DEBUG 
#define UID_NOT_FOUND 0xFFFF

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


Sequencer::Sequencer() :  listener( NULL ), notifier( NULL ), authresolver(NULL),
fuid( 1 ), startTime ( Messaging::getTime() )
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
void Sequencer::initilize()
{
    STACKLOG;
    
    Sequencer* instance  = Instance();
	instance->clients.reserve( Config::getMaxClients() );
	instance->listener = new Listener(Config::getListenPort());

	pthread_create(&instance->killerthread, NULL, s_klthreadstart, &instance);

	if( Config::getServerMode() != SERVER_LAN )
	{
		instance->notifier = new Notifier();

		// only start userauth if we are registered with the master server and if we have trustworthyness > 1
		if(instance->notifier->getAdvertised() && instance->notifier->getTrustLevel()>1)
			instance->authresolver = new UserAuth(instance->notifier->getChallenge());

	}
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::cleanUp()
{
    STACKLOG;

    Sequencer* instance = Instance();
	for( unsigned int i = 0; i < instance->clients.size(); i++) 
	{
		disconnect(instance->clients[i]->uid, "server shutting down");
	}
	
	if( instance->notifier )
		delete instance->notifier;
	
	delete instance->listener;
	delete instance->mInstance;
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
    STACKLOG;
	// WARNING: be sure that this is only called within a clients_mutex lock!
	
	// check for duplicate names
	Sequencer* instance = Instance();
	for (unsigned int i = 0; i < instance->clients.size(); i++)
	{
		if (!strcmp(nick, instance->clients[i]->nickname)) 
		{
			return true;
		}
	}
	return false;
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, user_credentials_t *user)
{
    STACKLOG;
    Sequencer* instance = Instance();
	//we have a confirmed client that wants to play
	//try to find a place for him
	Logger::log(LOG_DEBUG,"got instance in createClient()");

    MutexLocker scoped_lock(instance->clients_mutex);
	bool dupeNick = Sequencer::checkNickUnique(user->username);
	int dupecounter = 2;

	// check if server is full
	Logger::log(LOG_WARN,"searching free slot for new client...");
	if( instance->clients.size() >= Config::getMaxClients() )
	{
		Logger::log(LOG_WARN,"join request from '%s' on full server: rejecting!", user->username);
		// set a low time out because we don't want to cause a back up of
		// connecting clients
		sock->set_timeout( 10, 0 );
		Messaging::sendmessage(sock, MSG2_FULL, 0, 0, 0);
		throw std::runtime_error("Server is full");
	}
	
	if(dupeNick)
	{
		char buf[20] = "";
		strncpy(buf, user->username, 20);
		Logger::log(LOG_WARN,"found duplicate nick, getting new one: %s", buf);
		if(strnlen(buf, 20) == 20)
			//shorten the string
			buf[18]=0;
		while(dupeNick)
		{
			sprintf(buf+strnlen(buf, 18), "%d", dupecounter++);
			Logger::log(LOG_DEBUG,"checked for duplicate nick (2): %s", buf);
			dupeNick = Sequencer::checkNickUnique(buf);
		}
		Logger::log(LOG_WARN,"chose alternate username: %s\n", buf);
		strncpy(user->username, buf, 20);
		
		// we should send him a message about the nickchange later...
	}
	
	
	client_t* to_add = new client_t;
	
	//okay, create the stuff
	to_add->flow=false;
	to_add->status=USED;
	to_add->vehicle_name[0]=0;
	to_add->position=Vector3(0,0,0);
	to_add->rconretries=0;

	// auth stuff
	to_add->rconauth=0;
	if(instance->authresolver)
	{
		int res = instance->authresolver->getUserModeByUserToken(user->uniqueid);
		if(res>0)
		{
			Logger::log(LOG_INFO, "user authed because of valid admin token!");
			to_add->rconauth=res;
		}
	}

	memset(to_add->nickname, 0, 32); // for 20 character long nicknames :)
	strncpy(to_add->nickname, user->username, 20);
	strncpy(to_add->uniqueid, user->uniqueid, 60);
	to_add->receiver = new Receiver();
	to_add->broadcaster = new Broadcaster();

	// replace bad characters
	for (unsigned int i=0; i<20; i++)
	{
		if(to_add->nickname[i] == 0)
			break;
	}

	to_add->uid=instance->fuid;
	instance->fuid++;
	to_add->sock = sock;//this won't interlock
	
	instance->clients.push_back( to_add );
	to_add->receiver->reset(to_add->uid, sock);
	to_add->broadcaster->reset(to_add->uid, sock,
			Sequencer::disconnect, Messaging::sendmessage);

	Logger::log(LOG_VERBOSE,"Sequencer: New client added");
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
		for( unsigned int i = 0; i < instance->clients.size(); i++)
		{
			char playerdata[1024] = "";
			char positiondata[128] = "";
			instance->clients[i]->position.toString(positiondata);
			sprintf(playerdata, "%d;%s;%s;%s;%s;%s\n", i, 
					instance->clients[i]->vehicle_name, 
					instance->clients[i]->nickname,
					positiondata, 
					instance->clients[i]->sock->get_peerAddr(&error).c_str(),
					instance->clients[i]->uniqueid);
			strcat(hearbeatdata, playerdata);
		}
	}
	return 0;
}

int Sequencer::getNumClients()
{
    STACKLOG;
    Sequencer* instance = Instance();
	MutexLocker scoped_lock(instance->clients_mutex);
	return (int)instance->clients.size();
}

int Sequencer::authNick(std::string token, std::string &nickname)
{
    STACKLOG;
    Sequencer* instance = Instance();
	MutexLocker scoped_lock(instance->clients_mutex);
	if(!instance->authresolver)
		return -1;
	return instance->authresolver->resolve(token, nickname);
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
		while( instance->killqueue.empty() )
			instance->killer_mutex.wait(instance->killer_cv);
		
		//pop the kill queue
		client_t* to_del = instance->killqueue.front();
		instance->killqueue.pop();
		instance->killer_mutex.unlock();

		Logger::log(LOG_DEBUG,"Killer called to kill %s", to_del->nickname );
		// CRITICAL ORDER OF EVENTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// stop the broadcaster first, then disconnect the socket.
		// other wise there is a chance (being concurrent code) that the
		// socket will attempt to send a message between the disconnect
		// which makes the socket invalid) and the actual time of stoping
		// the bradcaster
		to_del->broadcaster->stop();
		to_del->receiver->stop();
        to_del->sock->disconnect(&error);
		// END CRITICAL ORDER OF EVENTS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		delete to_del->broadcaster;
		delete to_del->receiver;
		delete to_del->sock;
		to_del->broadcaster = NULL;
		to_del->receiver = NULL;
		to_del->sock = NULL;
		
		delete to_del;
		to_del = NULL;
	}
}

void Sequencer::disconnect(int uid, const char* errormsg)
{
    STACKLOG;
    Sequencer* instance = Instance(); 
    
    MutexLocker scoped_lock(instance->killer_mutex);
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;
    
	//this routine is a potential trouble maker as it can be called from many thread contexts
	//so we use a killer thread
	Logger::log(LOG_VERBOSE, "Disconnecting Slot %d: %s", pos, errormsg);
	
	client_t *c = instance->clients[pos];
	instance->killqueue.push(c);
	
	//notify the others
	for( unsigned int i = 0; i < instance->clients.size(); i++)
	{
		if( strlen(instance->clients[i]->vehicle_name) > 0 )
		{
			char *forced = "forced";
			instance->clients[i]->broadcaster->queueMessage(
					instance->clients[pos]->uid, MSG2_DELETE, (int)strlen(forced), forced);
		}
	}
	instance->clients.erase( instance->clients.begin() + pos );
	instance->killer_cv.signal();
    
    printStats();
}

//this is called from the listener thread initial handshake
void Sequencer::enableFlow(int uid)
{
    STACKLOG;
    Sequencer* instance = Instance();
    
    MutexLocker scoped_lock(instance->clients_mutex); 
    unsigned short pos = instance->getPosfromUid(uid);    
    if( UID_NOT_FOUND == pos ) return;
    
	instance->clients[pos]->flow=true;
	// now they are a bonified part of the server, show the new stats
    printStats();
}


//this is called from the listener thread initial handshake
int Sequencer::sendMOTD(int uid)
{
    STACKLOG;

	std::vector<std::string> lines;
	int res = readFile("motd.txt", lines);
	if(res)
		return res;

	std::vector<std::string>::iterator it;
	for(it=lines.begin(); it!=lines.end(); it++)
	{
		serverSay(*it, uid, 1);
	}
	return 0;
}

int Sequencer::readFile(std::string filename, std::vector<std::string> &lines)
{
	FILE *f = fopen(filename.c_str(), "r");
	if (!f)
		return -1;
	int linecounter=0;
	while(!feof(f))
	{
		char line[2048] = "";
		memset(line, 0, 2048);
		fgets (line, 2048, f);
		linecounter++;
		
		if(strnlen(line, 2048) <= 2)
			continue;
		
		// strip line (newline char)
		char *ptr = line;
		while(*ptr)
		{
			if(*ptr == '\n')
			{
				*ptr=0;
				break;
			}
			ptr++;
		}
		lines.push_back(std::string(line));
	}
	fclose (f);
	return 0;
}

//this is called from the listener thread initial handshake
void Sequencer::notifyAllVehicles(int uid)
{
    STACKLOG;
    Sequencer* instance = Instance();
    
    MutexLocker scoped_lock(instance->clients_mutex); 
    unsigned short pos = instance->getPosfromUid(uid);     
    if( UID_NOT_FOUND == pos ) return;
    
	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (i!=pos && instance->clients[i]->status == USED &&
				strlen(instance->clients[i]->vehicle_name)>0)
		{
			char message[512];
			strcpy(message, instance->clients[i]->vehicle_name);
			strcpy(message + strlen(instance->clients[i]->vehicle_name) + 1,
					instance->clients[i]->nickname);
			instance->clients[pos]->broadcaster->queueMessage(
					instance->clients[i]->uid, MSG2_USE_VEHICLE,
					((int)(strlen(instance->clients[i]->vehicle_name) +
						strlen(instance->clients[i]->nickname) + 2 )),
					message );
		}
		// not possible to have flow enabled but not have a truck... disconnect 
		if ( !strlen(instance->clients[i]->vehicle_name) &&
				instance->clients[i]->flow )
		{
			Logger::log(LOG_ERROR, "Client has flow enable but no truck name, "
					"disconnecting");
			disconnect(instance->clients[i]->uid, "client appears to be disconnected");
		}
	}
}

// this does not lock the clients_mutex, make sure it is locked before hand
void Sequencer::serverSay(std::string msg, int uid, int type)
{
    STACKLOG;
    Sequencer* instance = Instance(); 
	if(type==0)
		msg = std::string("SERVER: ") + msg;

	for (int i = 0; i < (int)instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED &&
				instance->clients[i]->flow &&
				(uid==-1 || instance->clients[i]->uid == uid))
			instance->clients[i]->broadcaster->queueMessage(
					-1, MSG2_CHAT,
					(int)msg.size(),
					msg.c_str() );
	}
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int uid, int type, char* data, unsigned int len)
{
	STACKLOG;
    Sequencer* instance = Instance();

    MutexLocker scoped_lock(instance->clients_mutex);
    unsigned short pos = instance->getPosfromUid(uid);    
    if( UID_NOT_FOUND == pos ) return;
    
	int publishMode=0;
	// publishMode = 0 no broadcast
	// publishMode = 1 broadcast to all clients
	// publishMode = 2 broadcast to authed users (bots)

	if (type==MSG2_USE_VEHICLE) 
	{
		data[len]=0;
		strncpy(instance->clients[pos]->vehicle_name, data, 129);
		Logger::log(LOG_VERBOSE,"On the fly vehicle registration for slot %d: %s",
		            pos, instance->clients[pos]->vehicle_name);
		//we alter the message to add user info
		strcpy(data + len + 1, instance->clients[pos]->nickname);
		len += (int)strlen(instance->clients[pos]->nickname) + 2;
		publishMode = 1;
	}
	
	else if (type==MSG2_DELETE)
	{
		static int counter_crash=0, counter_deletes=0;
		counter_deletes++;
		if(len==0)
		{
			// from client
			Logger::log(LOG_INFO, "user %s disconnects on request", instance->clients[pos]->nickname);

			char tmp[1024];
			sprintf(tmp, "user %s disconnects on request", instance->clients[pos]->nickname);
			serverSay(std::string(tmp), -1);
			disconnect(instance->clients[pos]->uid, "disconnected on request");
		}else
		{
			counter_crash++;
			char tmp[1024];
			sprintf(tmp, "user %s crashed D:", instance->clients[pos]->nickname);
			serverSay(std::string(tmp), -1);
			disconnect(instance->clients[pos]->uid, "disconnected on crash");
		}
		Logger::log(LOG_INFO, "crash statistic: %d of %d deletes crashed", counter_crash, counter_deletes);
	}
	else if (type==MSG2_RCON_COMMAND)
	{
		Logger::log(LOG_WARN, "user %d (%d) sends rcon command: %s",
				pos,
				instance->clients[pos]->rconauth,
				data);
		if(instance->clients[pos]->rconauth>0)
		{
			if(instance->clients[pos]->rconauth>1)
			{
				// bot commands
				if(!strncmp(data, "newgoal", 4))
				{
					float x=0, y=0, z=0;
					int userid=0;
					char text[255];
					memset(text, 0, 254);
					int res = sscanf(data, "newgoal %d, %f, %f, %f, %s", &userid, &x, &y, &z, text);
					if(res < 5)
					{
						// discard, as its invalid
						return;
					}
					char txtbuf[1024] = "";
					memset(txtbuf, 0, 1024);
					sprintf(txtbuf, "newgoal %f, %f, %f, %s", x, y, z, text);
					int pos = instance->getPosfromUid(userid);
				    if( UID_NOT_FOUND == pos ) {
				    	const char *error = "no user with that ID found!";
				    	instance->clients[pos]->broadcaster-> queueMessage(
							0, MSG2_RCON_COMMAND_FAILED, 
							(int)strlen(error), error );
				    } else {
				    		instance->clients[pos]->broadcaster->queueMessage(
							-1, MSG2_GAME_CMD, 
							(int)strlen(txtbuf), txtbuf);
					}
					return;
				}
				if(!strncmp(data, "createoverlay", 13))
				{
					int userid=0;
					int res = sscanf(data, "createoverlay %04d", &userid);
					if(res < 1)
					{
						// discard, as its invalid
						return;
					}
					char newbuf[8192] = ""; // max buffer size of RoR also
					strcpy(newbuf, "createoverlay ");
					strncpy(newbuf+14, data + 18, len-18);
					int pos = instance->getPosfromUid(userid);
				    if( UID_NOT_FOUND == pos ) {
				    	const char *error = "no user with that ID found!";
				    	instance->clients[pos]->broadcaster-> queueMessage(
							0, MSG2_RCON_COMMAND_FAILED, 
							(int)strlen(error), error );
				    } else {
						instance->clients[pos]->broadcaster->queueMessage(
							-1, MSG2_GAME_CMD,
							(int)strlen(newbuf), newbuf);
					}
					return;
				}
				if(!strncmp(data, "setoverlayvisible", 17))
				{
					char oname[255];
					int visible=0;
					int userid=0;
					memset(oname, 0, 254);
					int res = sscanf(data, "setoverlayvisible %d, %d, %s", &userid, &visible, oname);
					if(res < 3)
					{
						// discard, as its invalid
						return;
					}
					char txtbuf[1024] = "";
					memset(txtbuf, 0, 1024);
					sprintf(txtbuf, "setoverlayvisible %d, %s", visible, oname);
					int pos = instance->getPosfromUid(userid);
				    if( UID_NOT_FOUND == pos ) {
				    	const char *error = "no user with that ID found!";
				    	instance->clients[pos]->broadcaster-> queueMessage(
							0, MSG2_RCON_COMMAND_FAILED, 
							(int)strlen(error), error );
				    } else {
						instance->clients[pos]->broadcaster->queueMessage(
							-1, MSG2_GAME_CMD,
							(int)strlen(txtbuf), txtbuf);
					}
				}
				if(!strncmp(data, "setoverlayelementcolor", 22))
				{
					char oname[255];
					int userid=0;
					float r=0, g=0, b=0, a=0;
					memset(oname, 0, 254);
					int res = sscanf(data, "setoverlayelementcolor %d, %f, %f, %f, %f, %s", &userid, &r, &g, &b, &a, oname);
					if(res < 6)
					{
						// discard, as its invalid
						return;
					}
					char txtbuf[1024] = "";
					memset(txtbuf, 0, 1024);
					sprintf(txtbuf, "setoverlayelementcolor %f, %f, %f, %f, %s", r, g, b, a, oname);
					
					int pos = instance->getPosfromUid(userid);
				    if( UID_NOT_FOUND == pos ) {
						const char *error = "no user with that ID found!";
						instance->clients[pos]->broadcaster->queueMessage(
								0, MSG2_RCON_COMMAND_FAILED,
								(int)strlen(error), error );
					} else {
						instance->clients[pos]->broadcaster->queueMessage(
							-1, MSG2_GAME_CMD, 
							(int)strlen(txtbuf), txtbuf);
					}
				}
				if(!strncmp(data, "setoverlayelementtext", 21))
				{
					char oname[255];
					memset(oname, 0, 254);
					char text[1024];
					memset(text, 0, 1023);
					int userid=0;
					int res = sscanf(data, "setoverlayelementtext %d %s %s", &userid, text, oname);
					if(res < 3)
					{
						// discard, as its invalid
						return;
					}
					char txtbuf[1024] = "";
					memset(txtbuf, 0, 1024);
					sprintf(txtbuf, "setoverlayelementtext %s %s", text, oname);
					int pos = instance->getPosfromUid(userid);
				    if( UID_NOT_FOUND == pos ) {
						const char *error = "no user with that ID found!";
						instance->clients[pos]->broadcaster->queueMessage(
							0, MSG2_RCON_COMMAND_FAILED,
							(int)strlen(error), error );
					} else {
						instance->clients[pos]->broadcaster->queueMessage(
							-1, MSG2_GAME_CMD,
							(int)strlen(txtbuf), txtbuf);
					}
				}
			}
			if(!strncmp(data, "kick", 4))
			{
				int player = -1;
				int res = sscanf(data, "kick %d", &player); 
				if(res == 1 && player != -1 && player < (int)instance->clients.size())
				{
					if(instance->clients[player]->status == FREE ||
							instance->clients[player]->status == BUSY)
					{
						const char *error = "cannot kick free or busy client";
						instance->clients[pos]->broadcaster->queueMessage(
								0, MSG2_RCON_COMMAND_FAILED, 
								(int)strlen(error), error );
					} else if(instance->clients[player]->status == USED) {
						Logger::log(LOG_WARN, "user %d kicked by user %d via rcmd",
								player, pos);
						char tmp[255]="";
						memset(tmp, 0, 255);
						sprintf(tmp, "player '%s' kicked successfully.",
								instance->clients[player]->nickname);
						serverSay(std::string(tmp));

						instance->clients[pos]->broadcaster->queueMessage(
								0, MSG2_RCON_COMMAND_SUCCESS, 
								(int)strlen(tmp), tmp );
						
						disconnect( instance->clients[player]->uid, "kicked" );
					}
				} else {
					const char *error = "invalid client number";
					instance->clients[pos]->broadcaster->queueMessage(
							0, MSG2_RCON_COMMAND_FAILED, 
							(int)strlen(error), error );
				}
			}
		}
		else
		{
			instance->clients[pos]->broadcaster->queueMessage( 0,
					MSG2_RCON_COMMAND_FAILED, 0, 0 );
			serverSay("rcon command unkown");
		}
		
		publishMode=0;
	}
	else if (type==MSG2_CHAT)
	{
		Logger::log(LOG_INFO, "CHAT| %s: %s", instance->clients[pos]->nickname, data);
		publishMode=1;
		if(data[0] == '!')
			// this enables bot commands that are not distributed
			publishMode=2;
		if(!strcmp(data, "!version"))
		{
			serverSay(std::string(VERSION), uid);
		}
	}
	else if (type==MSG2_PRIVCHAT)
	{
		// private chat message
		int destuid = *(int*)data;
		int destpos = instance->getPosfromUid(destuid);
		if(destpos != UID_NOT_FOUND)
		{
			char *chatmsg = data + sizeof(int);
			int chatlen = len - sizeof(int);
			instance->clients[destpos]->broadcaster->queueMessage(uid, MSG2_CHAT, chatlen, chatmsg);
			// use MSG2_PRIVCHAT later here maybe?
			publishMode=0;
		}
	}
	else if (type==MSG2_RCON_LOGIN)
	{
		if(instance->clients[pos]->rconauth != 0)
		{
			// already logged in
			instance->clients[pos]->broadcaster->queueMessage(
					0, MSG2_RCON_LOGIN_SUCCESS, 0, 0);
			return;
		}
		
		if( !Config::hasAdmin() && instance->clients[pos]->rconretries < 3)
		{
			char pw[255]="";
			strncpy(pw, data, 255);
			pw[len]=0;
			Logger::log(LOG_DEBUG, "user %d  tries to log into RCON: server: "
					"%s, his: %s", pos, Config::getAdminPassword().c_str(), pw);
			if( strnlen(pw, 250) > 20 && Config::getAdminPassword() != pw)
			{
				Logger::log(LOG_WARN, "user %d logged into RCON", pos);
				instance->clients[pos]->rconauth=1;
				instance->clients[pos]->broadcaster->queueMessage( 
						0, MSG2_RCON_LOGIN_SUCCESS, 0, 0 );
			}else
			{
				// pw incorrect or failed
				Logger::log(LOG_WARN, "user %d failed to login RCON, retry "
						"number %d", pos, instance->clients[pos]->rconretries);
				instance->clients[pos]->rconauth=0;
				instance->clients[pos]->rconretries++;
				instance->clients[pos]->broadcaster->queueMessage( 0,
						MSG2_RCON_LOGIN_FAILED, 0, 0 );
				Messaging::sendmessage( instance->clients[pos]->sock,
						MSG2_RCON_LOGIN_FAILED, 0, 0, 0);
			}
		}else
		{
			Logger::log(LOG_WARN, "user %d failed to login RCON, as RCON is "
					"disabled %d", pos, instance->clients[pos]->rconretries);
			instance->clients[pos]->broadcaster->queueMessage( 0,
					MSG2_RCON_LOGIN_NOTAV, 0, 0 );
		}
		publishMode=0;
	}
	else if (type==MSG2_VEHICLE_DATA)
	{
		float* fpt=(float*)(data+sizeof(oob_t));
		instance->clients[pos]->position=Vector3(fpt[0], fpt[1], fpt[2]);
		publishMode=1;
	}
	else if (type==MSG2_FORCE)
	{
		//this message is to be sent to only one destination
		unsigned int destuid=((netforce_t*)data)->target_uid;
		for ( unsigned int i = 0; i < instance->clients.size(); i++)
		{
			if(i >= instance->clients.size())
				break;
			if (instance->clients[i]->status == USED && 
				instance->clients[i]->flow &&
				instance->clients[i]->uid==destuid)
				instance->clients[i]->broadcaster->queueMessage(
						instance->clients[pos]->uid, type, len, data);
		}
		publishMode=0;
	}
	
	if(publishMode>0)
	{
		if(publishMode == 1)
		{
			// just push to all the present clients
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				if (instance->clients[i]->status == USED && instance->clients[i]->flow && i!=pos)
					instance->clients[i]->broadcaster->queueMessage(instance->clients[pos]->uid, type, len, data);
			}
		} else if(publishMode == 2)
		{
			// push to all bots and authed users above auth level 1
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				if (instance->clients[i]->status == USED && instance->clients[i]->flow && i!=pos && instance->clients[i]->rconauth > 1)
					instance->clients[i]->broadcaster->queueMessage(instance->clients[pos]->uid, type, len, data);
			}
		}
	}
}

// clients_mutex needs to be locked wen calling this method
void Sequencer::printStats()
{
    STACKLOG;
    Sequencer* instance = Instance();
	SWBaseSocket::SWBaseError error;
#ifdef NCURSES
	if(instance->guimode)
	{
		mvwprintw(win_slots, 1, 1, "Slot Status UID IP              Nickname");
		mvwprintw(win_slots, 2, 1, "------------------------------------------");
		for (int i=0; i<clients.size(); i++)
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
		for (unsigned int i = 0; i < instance->clients.size(); i++)
		{
			if (instance->clients[i]->status == FREE) 
				Logger::log(LOG_INFO, "%4i Free", i);
			else if (instance->clients[i]->status == BUSY)
				Logger::log(LOG_INFO, "%4i Busy %5i %-16s %s, %s", i,
						instance->clients[i]->uid, "-", 
						instance->clients[i]->nickname, 
						instance->clients[i]->vehicle_name);
			else 
				Logger::log(LOG_INFO, "%4i Used %5i %-16s %s, %s", i, 
						instance->clients[i]->uid, 
						instance->clients[i]->sock->get_peerAddr(&error).c_str(), 
						instance->clients[i]->nickname, 
						instance->clients[i]->vehicle_name);
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
unsigned short Sequencer::getPosfromUid(unsigned int uid)
{
    STACKLOG;
    Sequencer* instance = Instance();
    
    for (unsigned short i = 0; i < instance->clients.size(); i++)
    {
        if(instance->clients[i]->uid == uid)
            return i;
    }
    
    Logger::log( LOG_ERROR, "could not find uid %d", uid);    
    return UID_NOT_FOUND;
}

void Sequencer::unregisterServer()
{
	if( Instance()->notifier )
		Instance()->notifier->unregisterServer();
}

