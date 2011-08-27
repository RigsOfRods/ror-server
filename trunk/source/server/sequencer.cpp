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
#include "utils.h"
#include "ScriptEngine.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
//#define REFLECT_DEBUG
#define UID_NOT_FOUND 0xFFFF


#ifdef __GNUC__
#include <stdlib.h>
#endif


#include <cstdio>



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

unsigned int Sequencer::connCrash = 0;
unsigned int Sequencer::connCount = 0;


Sequencer::Sequencer() :  listener( NULL ), notifier( NULL ), authresolver(NULL),
fuid( 1 ), startTime ( Messaging::getTime() )
{
    STACKLOG;
}

Sequencer::~Sequencer()
{
	STACKLOG;
	//cleanUp();
}

/**
 * Inililize, needs to be called before the class is used
 */
void Sequencer::initilize()
{
    STACKLOG;

	if(mInstance)
		delete mInstance;
    Sequencer* instance  = Instance();
	instance->clients.reserve( Config::getMaxClients() );
	instance->listener = new Listener(Config::getListenPort());

	instance->script = 0;
#ifdef WITH_ANGELSCRIPT
	if(Config::getEnableScripting())
	{
		instance->script = new ScriptEngine(instance);
		instance->script->loadScript(Config::getScriptName());
	}
#endif //WITH_ANGELSCRIPT

	pthread_create(&instance->killerthread, NULL, s_klthreadstart, &instance);

	instance->authresolver = 0;
	instance->notifier = 0;
	if( Config::getServerMode() != SERVER_LAN )
	{
		instance->notifier = new Notifier(instance->authresolver);

		// start userauth
		instance->authresolver = new UserAuth(instance->notifier->getChallenge(), instance->notifier->getTrustLevel(), Config::getAuthFile());
	}
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::cleanUp()
{
    STACKLOG;

	static bool cleanup = false;
	if(cleanup) return;
	cleanup=true;

    Sequencer* instance = Instance();
	Logger::log(LOG_INFO,"closing. disconnecting clients ...");
	const char *str = "server shutting down (try to reconnect later!)";
	for( unsigned int i = 0; i < instance->clients.size(); i++)
	{
		// HACK-ISH override all thread stuff and directly send it!
		Messaging::sendmessage(instance->clients[i]->sock, MSG2_USER_LEAVE, instance->clients[i]->user.uniqueid, 0, strlen(str), str);
		//disconnect(instance->clients[i]->user.uniqueid, );
	}
	Logger::log(LOG_INFO,"all clients disconnected. exiting.");

	instance->notifier->unregisterServer();
	if(instance->notifier)
		delete instance->notifier;
	instance->notifier = 0;

#ifdef WITH_ANGELSCRIPT
	if(instance->script)
		delete instance->script;
#endif //WITH_ANGELSCRIPT

	if(instance->authresolver)
		delete instance->authresolver;
	
	delete instance->listener;

#ifndef WIN32
	sleep(2);
#else
	Sleep(2000);
#endif

	
	pthread_cancel(instance->killerthread);
	pthread_detach(instance->killerthread);
	delete instance->mInstance;
	mInstance = NULL;
	cleanup = false;
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
		if (!strcmp(nick, instance->clients[i]->user.username))
		{
			return true;
		}
	}
	return false;
}


int Sequencer::getFreePlayerColour()
{
    STACKLOG;
	// WARNING: be sure that this is only called within a clients_mutex lock!

	int col = 0;
	Sequencer* instance = Instance();
recheck_col:
	for (unsigned int i = 0; i < instance->clients.size(); i++)
	{
		if(instance->clients[i]->user.colournum == col)
		{
			col++;
			goto recheck_col;
		}
	}
	return col;
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, user_info_t user)
{
    STACKLOG;
    Sequencer* instance = Instance();
	//we have a confirmed client that wants to play
	//try to find a place for him
	Logger::log(LOG_DEBUG,"got instance in createClient()");

    MutexLocker scoped_lock(instance->clients_mutex);
	bool dupeNick = Sequencer::checkNickUnique(user.username);
	int playerColour = Sequencer::getFreePlayerColour();

	int dupecounter = 2;

	// check if banned
	SWBaseSocket::SWBaseError error;
	if(Sequencer::isbanned(sock->get_peerAddr(&error).c_str()))
	{
		Logger::log(LOG_VERBOSE,"rejected banned IP %s", sock->get_peerAddr(&error).c_str());
		Messaging::sendmessage(sock, MSG2_BANNED, 0, 0, 0, 0);
		return;
	}

	// check if server is full
	Logger::log(LOG_DEBUG,"searching free slot for new client...");
	if( instance->clients.size() >= Config::getMaxClients() )
	{
		Logger::log(LOG_WARN,"join request from '%s' on full server: rejecting!", user.username);
		// set a low time out because we don't want to cause a back up of
		// connecting clients
		sock->set_timeout( 10, 0 );
		Messaging::sendmessage(sock, MSG2_FULL, 0, 0, 0, 0);
		throw std::runtime_error("Server is full");
	}

	if(dupeNick)
	{
		char buf[255] = "";
		Logger::log(LOG_WARN,"found duplicate nick, getting new one: %s", buf);

		// shorten username so the number will fit
		user.username[16] = '\0';

		// now get a new number
		while(dupeNick)
		{
			sprintf(buf, "%s_%d", user.username, dupecounter++);
			buf[19] = '\0';
			//Logger::log(LOG_DEBUG,"checked for duplicate nick (2): %s", buf);
			dupeNick = Sequencer::checkNickUnique(buf);
		}
		Logger::log(LOG_WARN,"chose alternate username: %s\n", buf);
		strncpy(user.username, buf, 20);

		// we should send him a message about the nickchange later...
	}


	//okay, create the client slot
	client_t* to_add = new client_t;
	to_add->user            = user;
	to_add->flow            = false;
	to_add->status          = USED;
	to_add->initialized     = false;
	to_add->user.colournum  = playerColour;
	to_add->user.authstatus = user.authstatus;
	
	// log some info about this client
	Logger::log(LOG_INFO, "New client: %s (%s), using %s %s, with token %s", user.username, user.language, user.clientname, user.clientversion, std::string(user.usertoken).substr(0,40).c_str());

	// create new class instances for the receiving and sending thread
	to_add->receiver    = new Receiver();
	to_add->broadcaster = new Broadcaster();

	// replace bad characters
	for (unsigned int i=0; i<20; i++)
	{
		if(to_add->user.username[i] == 0)
			break;
		// replace all non-ASCII characters
		if(to_add->user.username[i] < 32 || to_add->user.username[i] > 125)
			to_add->user.username[i] = '?';
		// btw, the above code is disfunctional ...
	}

	// assign unique userid
	to_add->user.uniqueid = instance->fuid;

	// count up unique id
	instance->fuid++;

	to_add->sock = sock;//this won't interlock

	// add the client to the vector
	instance->clients.push_back( to_add );
	// create one thread for the receiver
	to_add->receiver->reset(to_add->user.uniqueid, sock);
	// and one for the broadcaster
	to_add->broadcaster->reset(to_add->user.uniqueid, 
								sock, 
								Sequencer::disconnect,
								Messaging::sendmessage,
								Messaging::addBandwidthDropOutgoing);

	// process slot infos
	int npos = instance->getPosfromUid(to_add->user.uniqueid);
	instance->clients[npos]->user.slotnum = npos;

	Logger::log(LOG_VERBOSE,"Sending welcome message to uid %i, slotpos: %i", instance->clients[npos]->user.uniqueid, npos);
	if( Messaging::sendmessage(sock, MSG2_WELCOME, instance->clients[npos]->user.uniqueid, 0, sizeof(user_info_t), (char *)&to_add->user) )
	{
		Sequencer::disconnect(instance->clients[npos]->user.uniqueid, "error sending welcome message" );
		return;
	}

	// notify everyone of the new client
	// but blank out the user token and GUID
	user_info_t info_for_others = to_add->user;
	memset(info_for_others.usertoken, 0, 40);
	memset(info_for_others.clientGUID, 0, 40);
	for(unsigned int i = 0; i < instance->clients.size(); i++)
	{
		instance->clients[i]->broadcaster->queueMessage(MSG2_USER_JOIN, instance->clients[npos]->user.uniqueid, 0, sizeof(user_info_t), (char*)&info_for_others);
	}

	// done!
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
	                      "version5\n" \
	                      "%i\n", challenge, clientnum);
	if(clientnum > 0)
	{
		for( unsigned int i = 0; i < instance->clients.size(); i++)
		{
			char authst[10] = "";
			if(instance->clients[i]->user.authstatus & AUTH_ADMIN) strcat(authst, "A");
			if(instance->clients[i]->user.authstatus & AUTH_MOD) strcat(authst, "M");
			if(instance->clients[i]->user.authstatus & AUTH_RANKED) strcat(authst, "R");
			if(instance->clients[i]->user.authstatus & AUTH_BOT) strcat(authst, "B");

			char playerdata[1024] = "";
			sprintf(playerdata, "%d;%s;%s;%s;%d\n",
					i,
					instance->clients[i]->user.username,
					instance->clients[i]->sock->get_peerAddr(&error).c_str(),
					authst,
					(int)instance->clients[i]->user.uniqueid
					);
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
		return AUTH_NONE;
	return instance->authresolver->resolve(token, nickname, instance->fuid);
}

ScriptEngine* Sequencer::getScriptEngine()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->script;
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

		Logger::log(LOG_DEBUG,"Killer called to kill %s", to_del->user.username );
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

void Sequencer::disconnect(int uid, const char* errormsg, bool isError)
{
    STACKLOG;
    Sequencer* instance = Instance();

    MutexLocker scoped_lock(instance->killer_mutex);
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;

	// send an event if user is rankend and if we are a official server
	if(instance->authresolver && (instance->clients[pos]->user.authstatus & AUTH_RANKED))
	{
		instance->authresolver->sendUserEvent(instance->clients[pos]->user.usertoken, (isError?"crash":"leave"), instance->clients[pos]->user.username, "");
	}

#ifdef WITH_ANGELSCRIPT
	if(instance->script)
		instance->script->playerDeleted(instance->clients[pos]->user.uniqueid, isError?1:0);
#endif //WITH_ANGELSCRIPT

	//this routine is a potential trouble maker as it can be called from many thread contexts
	//so we use a killer thread
	Logger::log(LOG_VERBOSE, "Disconnecting Slot %d: %s", pos, errormsg);

	client_t *c = instance->clients[pos];

	Logger::log(LOG_DEBUG, "adding client to kill queue, size: %d", instance->killqueue.size());
	instance->killqueue.push(c);

	//notify the others
	for( unsigned int i = 0; i < instance->clients.size(); i++)
	{
		instance->clients[i]->broadcaster->queueMessage(MSG2_USER_LEAVE, instance->clients[pos]->user.uniqueid, 0, (int)strlen(errormsg), errormsg);
	}
	instance->clients.erase( instance->clients.begin() + pos );

	instance->killer_cv.signal();


	instance->connCount++;
	if(isError)
		instance->connCrash++;
	Logger::log(LOG_INFO, "crash statistic: %d of %d deletes crashed", instance->connCrash, instance->connCount);

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
	int res = readFile(Config::getMOTDFile(), lines);
	if(res)
		return res;

	std::vector<std::string>::iterator it;
	for(it=lines.begin(); it!=lines.end(); it++)
	{
		serverSay(*it, uid, FROM_MOTD);
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


UserAuth* Sequencer::getUserAuth()
{
	STACKLOG;
	Sequencer* instance = Instance();
	return instance->authresolver;
}

//this is called from the listener thread initial handshake
void Sequencer::notifyAllVehicles(int uid, bool lock)
{
    STACKLOG;
    Sequencer* instance = Instance();

	if(lock)
		MutexLocker scoped_lock(instance->clients_mutex);

    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;

	user_info_t info_for_others = instance->clients[pos]->user;
	memset(info_for_others.usertoken, 0, 40);
	memset(info_for_others.clientGUID, 0, 40);

	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED)
		{
			// send user infos

			// new user to all others
			instance->clients[i]->broadcaster->queueMessage(MSG2_USER_INFO, instance->clients[pos]->user.uniqueid, 0, sizeof(user_info_t), (char*)&info_for_others);

			// all others to new user
			user_info_t info_for_others2 = instance->clients[i]->user;
			memset(info_for_others2.usertoken, 0, 40);
			memset(info_for_others2.clientGUID, 0, 40);
			instance->clients[pos]->broadcaster->queueMessage(MSG2_USER_INFO, instance->clients[i]->user.uniqueid, 0, sizeof(user_info_t), (char*)&info_for_others2);

			Logger::log(LOG_VERBOSE, " * %d streams registered for user %d", instance->clients[i]->streams.size(), instance->clients[i]->user.uniqueid);
			for(std::map<unsigned int, stream_register_t>::iterator it = instance->clients[i]->streams.begin(); it!=instance->clients[i]->streams.end(); it++)
			{
				Logger::log(LOG_VERBOSE, "sending stream registration %d:%d to user %d", instance->clients[i]->user.uniqueid, it->first, uid);
				instance->clients[pos]->broadcaster->queueMessage(MSG2_STREAM_REGISTER, instance->clients[i]->user.uniqueid, it->first, sizeof(stream_register_t), (char*)&it->second);
			}

		}
	}
}

int Sequencer::sendGameCommand(int uid, std::string cmd)
{
    STACKLOG;
    Sequencer* instance = Instance();
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return -1;
	// send
	const char *data = cmd.c_str();
	int size = cmd.size();
	// -1 = comes from the server
	instance->clients[pos]->broadcaster->queueMessage(MSG2_GAME_CMD, -1, 0, size, data);
	return 0;
}

// this does not lock the clients_mutex, make sure it is locked before hand
// note: uid=-1 = broadcast your message to all players
void Sequencer::serverSay(std::string msg, int uid, int type)
{
    STACKLOG;
    Sequencer* instance = Instance();

	if(type==FROM_SERVER)
		msg = std::string("SERVER: ") + msg;
	if(type==FROM_HOST) {
		if(uid==-1)
			msg = std::string("Host(general): ") + msg;
		else
			msg = std::string("Host(private): ") + msg;
	}
	if(type==FROM_RULES)
		msg = std::string("Rules: ") + msg;
	if(type==FROM_MOTD)
		msg = std::string("MOTD: ") + msg;

	for (int i = 0; i < (int)instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED &&
				instance->clients[i]->flow &&
				(uid==-1 || ((int)instance->clients[i]->user.uniqueid) == uid))
			instance->clients[i]->broadcaster->queueMessage(MSG2_CHAT, -1, -1, (int)msg.size(), msg.c_str() );
	}
}

void Sequencer::serverSayThreadSave(std::string msg, int uid, int type)
{
    STACKLOG;
    Sequencer* instance = Instance();
    //MutexLocker scoped_lock(instance->clients_mutex);
	instance->serverSay(msg, uid, type);
}

bool Sequencer::kick(int kuid, int modUID, const char *msg)
{
    STACKLOG;
    Sequencer* instance = Instance();
    unsigned short pos = instance->getPosfromUid(kuid);
    if( UID_NOT_FOUND == pos ) return false;
    unsigned short posMod = instance->getPosfromUid(modUID);
    if( UID_NOT_FOUND == posMod ) return false;

	char kickmsg[1024] = "";
	strcat(kickmsg, "kicked by ");
	strcat(kickmsg, instance->clients[posMod]->user.username);
	if(msg)
	{
		strcat(kickmsg, " for ");
		strcat(kickmsg, msg);
	}
	
	char kickmsg2[1024] = "";
	sprintf(kickmsg2, "player %s was %s",instance->clients[pos]->user.username, kickmsg);
	serverSay(kickmsg2, -1, FROM_SERVER);
	
	Logger::log(LOG_VERBOSE, "player '%s' kicked by '%s'",instance->clients[pos]->user.username, instance->clients[posMod]->user.username);
	disconnect(instance->clients[pos]->user.uniqueid, kickmsg);
	return true;
}

bool Sequencer::ban(int buid, int modUID, const char *msg)
{
    STACKLOG;
    Sequencer* instance = Instance();
    unsigned short pos = instance->getPosfromUid(buid);
    if( UID_NOT_FOUND == pos ) return false;
    unsigned short posMod = instance->getPosfromUid(modUID);
    if( UID_NOT_FOUND == posMod ) return false;
	SWBaseSocket::SWBaseError error;

	// construct ban data and add it to the list
	ban_t* b = new ban_t;
	memset(b, 0, sizeof(ban_t));

	b->uid = buid;
	if(msg) strncpy(b->banmsg, msg, 256);
	strncpy(b->bannedby_nick, instance->clients[posMod]->user.username, 20);
	strncpy(b->ip, instance->clients[pos]->sock->get_peerAddr(&error).c_str(), 16);
	strncpy(b->nickname, instance->clients[pos]->user.username, 20);
	Logger::log(LOG_DEBUG, "adding ban, size: %d", instance->bans.size());
	instance->bans.push_back(b);
	Logger::log(LOG_VERBOSE, "new ban added '%s' by '%s'", instance->clients[pos]->user.username, instance->clients[posMod]->user.username);

	char tmp[1024]="";
	if(msg)
	{
		strcat(tmp, msg);
	}
	strcat(tmp, " (banned)");

	return kick(buid, modUID, tmp);
}

void Sequencer::ban(int buid, const char *msg)
{
	STACKLOG;
	Sequencer* instance = Instance();
	unsigned short pos = instance->getPosfromUid(buid);
	if( UID_NOT_FOUND != pos )
	{
		SWBaseSocket::SWBaseError error;

		// construct ban data and add it to the list
		ban_t* b = new ban_t;
		memset(b, 0, sizeof(ban_t));

		b->uid = buid;
		if(msg) strncpy(b->banmsg, msg, 256);
		strncpy(b->bannedby_nick, "rorserver", 20);
		strncpy(b->ip, instance->clients[pos]->sock->get_peerAddr(&error).c_str(), 16);
		strncpy(b->nickname, instance->clients[pos]->user.username, 20);
		Logger::log(LOG_DEBUG, "adding ban, size: %d", instance->bans.size());
		instance->bans.push_back(b);
		Logger::log(LOG_VERBOSE, "new ban added '%s' by rorserver", instance->clients[pos]->user.username);

		char tmp[1024]="";
		if(msg)
			strcat(tmp, msg);
		strcat(tmp, " (banned)");

		disconnect(instance->clients[pos]->user.uniqueid, tmp);
	}
	else
		Logger::log(LOG_ERROR, "void Sequencer::ban(%d, %s) --> uid %d not found!", buid, msg, buid);
}

bool Sequencer::unban(int buid)
{
    STACKLOG;
    Sequencer* instance = Instance();
	for (unsigned int i = 0; i < instance->bans.size(); i++)
	{
		if(((int)instance->bans[i]->uid) == buid)
		{
			instance->bans.erase(instance->bans.begin() + i);
			Logger::log(LOG_VERBOSE, "uid unbanned: %d", buid);
			return true;
		}
	}
	return false;
}

bool Sequencer::isbanned(const char *ip)
{
	if(!ip) return false;
    STACKLOG;
    Sequencer* instance = Instance();
	for (unsigned int i = 0; i < instance->bans.size(); i++)
	{
		if(!strcmp(instance->bans[i]->ip, ip))
			return true;
	}
	return false;
}

void Sequencer::streamDebug()
{
    STACKLOG;
    Sequencer* instance = Instance();

    //MutexLocker scoped_lock(instance->clients_mutex);

	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED)
		{
			Logger::log(LOG_VERBOSE, " * %d %s (slot %d):", instance->clients[i]->user.uniqueid, instance->clients[i]->user.username, i);
			if(!instance->clients[i]->streams.size())
				Logger::log(LOG_VERBOSE, "  * no streams registered for user %d", instance->clients[i]->user.uniqueid);
			else
				for(std::map<unsigned int, stream_register_t>::iterator it = instance->clients[i]->streams.begin(); it!=instance->clients[i]->streams.end(); it++)
				{
					char *types[] = {(char *)"truck", (char *)"character", (char *)"aitraffic", (char *)"chat"};
					char *typeStr = (char *)"unkown";
					if(it->second.type>=0 && it->second.type <= 3)
						typeStr = types[it->second.type];
					Logger::log(LOG_VERBOSE, "  * %d:%d, type:%s status:%d name:'%s'", instance->clients[i]->user.uniqueid, it->first, typeStr, it->second.status, it->second.name);
				}
		}
	}
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int uid, int type, unsigned int streamid, char* data, unsigned int len)
{
	STACKLOG;
    Sequencer* instance = Instance();

    MutexLocker scoped_lock(instance->clients_mutex);
    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return;

	// check for full broadcaster queue
	{
		int dropstate = instance->clients[pos]->broadcaster->getDropState();
		if(true)
		{
			// queue full, inform client
			instance->clients[pos]->drop_state = dropstate;
			instance->clients[pos]->broadcaster->queueMessage(MSG2_NETQUALITY, -1, 0, sizeof(int), (char *)&dropstate);
		} else if(dropstate == 0 && instance->clients[pos]->drop_state == 1)
		{
			// queue working better again, inform client
			instance->clients[pos]->drop_state = dropstate;
			instance->clients[pos]->broadcaster->queueMessage(MSG2_NETQUALITY, -1, 0, sizeof(int), (char *)&dropstate);
		}
	}


	int publishMode=0;
	// publishMode = 0 no broadcast
	// publishMode = 1 broadcast to all clients except sender
	// publishMode = 2 broadcast to authed users (bots)
	// publishMode = 3 broadcast to all clients including sender

	if(type==MSG2_STREAM_DATA)
	{
		if(!instance->clients[pos]->initialized)
		{
			notifyAllVehicles(instance->clients[pos]->user.uniqueid, false);
			instance->clients[pos]->initialized=true;
		}

		publishMode = 1;
	}
	else if (type==MSG2_STREAM_REGISTER)
	{
		if(instance->clients[pos]->streams.size() >= Config::getMaxVehicles()+NON_VEHICLE_STREAMS)
		{
			// This user has too many vehicles, we drop the stream and then disconnect the user
			Logger::log(LOG_INFO, "%s(%d) has too many streams. Stream dropped, user kicked.", instance->clients[pos]->user.username, instance->clients[pos]->user.uniqueid);

			// send a message to the user.
			serverSay("You are now being kicked for having too many vehicles. Please rejoin.",  instance->clients[pos]->user.uniqueid, FROM_SERVER);

			// broadcast a general message that this user was auto-kicked
			char sayMsg[128] = "";
			sprintf(sayMsg, "%s was auto-kicked for having too many vehicles (limit: %d)", instance->clients[pos]->user.username, Config::getMaxVehicles());
			serverSay(sayMsg, -1, FROM_SERVER);
			disconnect(instance->clients[pos]->user.uniqueid, "You have too many vehicles. Please rejoin.", false);
			publishMode = 0; // drop
		}
		else
		{
			if( (instance->clients[pos]->streams.size() >= Config::getMaxVehicles()+NON_VEHICLE_STREAMS-3) && (instance->clients[pos]->streams.size() >= NON_VEHICLE_STREAMS) )
			{
				// we start warning the user as soon as he has only 3 vehicles left before he will get kicked.
				char sayMsg[128] = "";
				sprintf(sayMsg, "You do now have %d vehicles. The vehicle limit on this server is set on %d.", (instance->clients[pos]->streams.size()-1), Config::getMaxVehicles());
				serverSay(sayMsg, instance->clients[pos]->user.uniqueid, FROM_SERVER);
			}

			stream_register_t *reg = (stream_register_t *)data;
			Logger::log(LOG_VERBOSE, " * new stream registered: %d:%d, type: %d, name: '%s', status: %d", instance->clients[pos]->user.uniqueid, streamid, reg->type, reg->name, reg->status);
			for(int i=0;i<128;i++) if(reg->name[i] == ' ') reg->name[i] = 0; // convert spaces to zero's
			reg->name[127] = 0;
			instance->clients[pos]->streams[streamid] = *reg;

			// send an event if user is rankend and if we are a official server
			if(instance->authresolver && (instance->clients[pos]->user.authstatus & AUTH_RANKED))
				instance->authresolver->sendUserEvent(instance->clients[pos]->user.usertoken, std::string("newvehicle"), std::string(reg->name), std::string());

			instance->streamDebug();

			// reset some stats
			// streams_traffic limited through streams map
			instance->clients[pos]->streams_traffic[streamid].bandwidthIncoming=0;
			instance->clients[pos]->streams_traffic[streamid].bandwidthIncomingLastMinute=0;
			instance->clients[pos]->streams_traffic[streamid].bandwidthIncomingRate=0;
			instance->clients[pos]->streams_traffic[streamid].bandwidthOutgoing=0;
			instance->clients[pos]->streams_traffic[streamid].bandwidthOutgoingLastMinute=0;
			instance->clients[pos]->streams_traffic[streamid].bandwidthOutgoingRate=0;

			publishMode = 1;
		}
	}
	else if (type==MSG2_STREAM_REGISTER_RESULT)
	{
		// forward message to the stream origin
		stream_register_t *reg = (stream_register_t *)data;
		int origin_pos = instance->getPosfromUid(reg->origin_sourceid);
		if(origin_pos != UID_NOT_FOUND)
		{
			instance->clients[origin_pos]->broadcaster->queueMessage(type, uid, 0, sizeof(stream_register_t), (char *)reg);
			Logger::log(LOG_VERBOSE, "stream registration result for stream %03d:%03d from user %03d: %d", reg->origin_sourceid, reg->origin_streamid, uid, reg->status);
		}
		publishMode=0;
	}
	else if (type==MSG2_USER_LEAVE)
	{
		// from client
		Logger::log(LOG_INFO, "user %s disconnects on request", instance->clients[pos]->user.username);

		//char tmp[1024];
		//sprintf(tmp, "user %s disconnects on request", instance->clients[pos]->user.username);
		//serverSay(std::string(tmp), -1);
		disconnect(instance->clients[pos]->user.uniqueid, "disconnected on request", false);
	}
	else if (type == MSG2_CHAT)
	{
		Logger::log(LOG_INFO, "CHAT| %s: %s", instance->clients[pos]->user.username, data);
		publishMode=3;

		// no broadcast of server commands!
		if(data[0] == '!') publishMode=0;

#ifdef WITH_ANGELSCRIPT
		if(instance->script)
		{
			int scriptpub = instance->script->playerChat(instance->clients[pos]->user.uniqueid, data);
			if(scriptpub>0) publishMode = scriptpub;
		}
#endif //WITH_ANGELSCRIPT
		if(!strcmp(data, "!help"))
		{
			serverSay(std::string("builtin commands:"), uid);
			serverSay(std::string("!version, !list, !say, !bans, !ban, !unban, !kick, !vehiclelimit"), uid);
			serverSay(std::string("!website, !irc, !owner, !voip, !rules, !motd"), uid);
		}

		if(!strcmp(data, "!version"))
		{
			serverSay(std::string(VERSION), uid);
		}
		else if(!strcmp(data, "!list"))
		{
			serverSay(std::string(" uid | auth   | nick"), uid);
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				char authst[10] = "";
				if(instance->clients[i]->user.authstatus & AUTH_ADMIN) strcat(authst, "A");
				if(instance->clients[i]->user.authstatus & AUTH_MOD) strcat(authst, "M");
				if(instance->clients[i]->user.authstatus & AUTH_RANKED) strcat(authst, "R");
				if(instance->clients[i]->user.authstatus & AUTH_BOT) strcat(authst, "B");
				if(instance->clients[i]->user.authstatus & AUTH_BANNED) strcat(authst, "X");\

				char tmp2[256]="";
				sprintf(tmp2, "% 3d | %-6s | %-20s", instance->clients[i]->user.uniqueid, authst, instance->clients[i]->user.username);
				serverSay(std::string(tmp2), uid);
			}
		}
		else if(!strncmp(data, "!bans", 5))
		{
			serverSay(std::string("uid | IP              | nickname             | banned by"), uid);
			for (unsigned int i = 0; i < instance->bans.size(); i++)
			{
				char tmp[256]="";
				sprintf(tmp, "% 3d | %-15s | %-20s | %-20s", 
					instance->bans[i]->uid, 
					instance->bans[i]->ip, 
					instance->bans[i]->nickname, 
					instance->bans[i]->bannedby_nick);
				serverSay(std::string(tmp), uid);
			}
		}
		else if(!strncmp(data, "!unban", 6) && strlen(data) > 7)
		{
			if(instance->clients[pos]->user.authstatus & AUTH_MOD || instance->clients[pos]->user.authstatus & AUTH_ADMIN)
			{
				int buid=-1;
				int res = sscanf(data+7, "%d", &buid);
				if(res != 1 || buid == -1)
				{
					serverSay(std::string("usage: !unban <uid>"), uid);
					serverSay(std::string("example: !unban 3"), uid);
				} else
				{
					if(unban(buid))
						serverSay(std::string("ban not removed: error"), uid);
					else
						serverSay(std::string("ban removed"), uid);
				}
			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to unban people!"), uid);
			}
		}
		else if(!strncmp(data, "!ban ", 5) && strlen(data) > 5)
		{
			if(instance->clients[pos]->user.authstatus & AUTH_MOD || instance->clients[pos]->user.authstatus & AUTH_ADMIN)
			{
				int buid=-1;
				char banmsg_tmp[256]="";
				int res = sscanf(data+5, "%d %s", &buid, banmsg_tmp);
				std::string banMsg = std::string(banmsg_tmp);
				banMsg = trim(banMsg);
				if(res != 2 || buid == -1 || !banMsg.size())
				{
					serverSay(std::string("usage: !ban <uid> <message>"), uid);
					serverSay(std::string("example: !ban 3 swearing"), uid);
				} else
				{
					bool banned = ban(buid, uid, std::string(data).substr(6+intlen(buid),256).c_str());
					if(!banned)
						serverSay(std::string("kick + ban not successful: uid not found!"), uid);
				}
			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to ban people!"), uid);
			}
		}
		else if(!strncmp(data, "!kick ", 6) && strlen(data) > 6)
		{
			if(instance->clients[pos]->user.authstatus & AUTH_MOD || instance->clients[pos]->user.authstatus & AUTH_ADMIN)
			{
				int kuid=-1;
				char kickmsg_tmp[256]="";
				int res = sscanf(data+6, "%d %s", &kuid, kickmsg_tmp);
				std::string kickMsg = std::string(kickmsg_tmp);
				kickMsg = trim(kickMsg);
				if(res != 2 || kuid == -1 || !kickMsg.size())
				{
					serverSay(std::string("usage: !kick <uid> <message>"), uid);
					serverSay(std::string("example: !kick 3 bye!"), uid);
				} else
				{
					bool kicked  = kick(kuid, uid, std::string(data).substr(7+intlen(kuid),256).c_str());
					if(!kicked)
						serverSay(std::string("kick not successful: uid not found!"), uid);
				}
			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to kick people!"), uid);
			}
		}
		else if(!strcmp(data, "!vehiclelimit"))
		{
			char sayMsg[128] = "";
			sprintf(sayMsg, "The vehicle-limit on this server is set on %d", Config::getMaxVehicles());
			serverSay(sayMsg, uid, FROM_SERVER);
		}
		else if(!strncmp(data, "!say ", 5) && strlen(data) > 5)
		{
			if(instance->clients[pos]->user.authstatus & AUTH_MOD || instance->clients[pos]->user.authstatus & AUTH_ADMIN)
			{
				int kuid=-2;
				char saymsg_tmp[256]="";
				int res = sscanf(data+5, "%d %s", &kuid, saymsg_tmp);
				std::string sayMsg = std::string(saymsg_tmp);

				sayMsg = trim(sayMsg);
				if(res != 2 || kuid < -1 || !sayMsg.size())
				{
					serverSay(std::string("usage: !say <uid> <message> (use uid -1 for general broadcast)"), uid);
					serverSay(std::string("example: !say 3 Wecome to this server!"), uid);
				} else
				{
					serverSay(std::string(data).substr(6+intlen(kuid),256), kuid, FROM_HOST);
				}

			} else
			{
				// not allowed
				serverSay(std::string("You are not authorized to use this command!"), uid);
			}
		}
		else if(!strcmp(data, "!website") || !strcmp(data, "!www"))
		{
			if(!Config::getWebsite().empty())
			{
				char sayMsg[256] = "";
				sprintf(sayMsg, "Further information can be found online at %s", Config::getWebsite().c_str());
				serverSay(sayMsg, uid, FROM_SERVER);
			}
		}
		else if(!strcmp(data, "!irc"))
		{
			if(!Config::getIRC().empty())
			{
				char sayMsg[256] = "";
				sprintf(sayMsg, "IRC: %s", Config::getIRC().c_str());
				serverSay(sayMsg, uid, FROM_SERVER);
			}
		}
		else if(!strcmp(data, "!owner"))
		{
			if(!Config::getOwner().empty())
			{
				char sayMsg[256] = "";
				sprintf(sayMsg, "This server is run by %s", Config::getOwner().c_str());
				serverSay(sayMsg, uid, FROM_SERVER);
			}
		}
		else if(!strcmp(data, "!voip"))
		{
			if(!Config::getVoIP().empty())
			{
				char sayMsg[256] = "";
				sprintf(sayMsg, "This server's official VoIP: %s", Config::getVoIP().c_str());
				serverSay(sayMsg, uid, FROM_SERVER);
			}
		}
		else if(!strcmp(data, "!rules"))
		{
			if(!Config::getRulesFile().empty())
			{
				std::vector<std::string> lines;
				int res = readFile(Config::getRulesFile(), lines);
				if(!res)
				{
					std::vector<std::string>::iterator it;
					for(it=lines.begin(); it!=lines.end(); it++)
					{
						serverSay(*it, uid, FROM_RULES);
					}
				}
			}
		}
		else if(!strcmp(data, "!motd"))
		{
			sendMOTD(uid);
		}

		// add to chat log
		{
			time_t lotime = time(NULL);
			char timestr[50];
			memset(timestr, 0, 50);
			ctime_r(&lotime, timestr);
			// remove trailing new line
			timestr[strlen(timestr)-1]=0;

			if(instance->chathistory.size() > 500)
				instance->chathistory.pop_front();
			chat_save_t ch;
			ch.msg = std::string(data);
			ch.nick = std::string(instance->clients[pos]->user.username);
			ch.source = instance->clients[pos]->user.uniqueid;
			ch.time = std::string(timestr);
			instance->chathistory.push_back(ch);
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
			instance->clients[destpos]->broadcaster->queueMessage(uid, MSG2_CHAT, 1, chatlen, chatmsg);
			// use MSG2_PRIVCHAT later here maybe?
			publishMode=0;
		}
	}
#if 0
	// replaced with stream_data
	else if (type==MSG2_VEHICLE_DATA)
	{
#ifdef WITH_ANGELSCRIPT
		float* fpt=(float*)(data+sizeof(oob_t));
		instance->clients[pos]->position=Vector3(fpt[0], fpt[1], fpt[2]);
#endif //WITH_ANGELSCRIPT
		/*
		char hex[255]="";
		SHA1FromBuffer(hex, data, len);
		printf("R > %s\n", hex);

		std::string hexc = hexdump(data, len);
		printf("RH> %s\n", hexc.c_str());
		*/

		publishMode=1;
	}
#endif //0
#if 0
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
				instance->clients[i]->user.uniqueid==destuid)
				instance->clients[i]->broadcaster->queueMessage(
						instance->clients[pos]->user.uniqueid, type, len, data);
		}
		publishMode=0;
	}
#endif //0
	if(publishMode>0)
	{
		instance->clients[pos]->streams_traffic[streamid].bandwidthIncoming += len;

		
		if(publishMode == 1 || publishMode == 3)
		{
			bool toAll = (publishMode == 3);
			// just push to all the present clients
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				if (instance->clients[i]->status == USED && instance->clients[i]->flow && (i!=pos || toAll))
				{
					instance->clients[i]->streams_traffic[streamid].bandwidthOutgoing += len;
					instance->clients[i]->broadcaster->queueMessage(type, instance->clients[pos]->user.uniqueid, streamid, len, data);
				}
			}
		} else if(publishMode == 2)
		{
			// push to all bots and authed users above auth level 1
			for (unsigned int i = 0; i < instance->clients.size(); i++)
			{
				if(i >= instance->clients.size())
					break;
				if (instance->clients[i]->status == USED && instance->clients[i]->flow && i!=pos && (instance->clients[i]->user.authstatus & AUTH_ADMIN))
				{
					instance->clients[i]->streams_traffic[streamid].bandwidthOutgoing += len;
					instance->clients[i]->broadcaster->queueMessage(type, instance->clients[pos]->user.uniqueid, streamid, len, data);
				}
			}
		}
	}
}

Notifier *Sequencer::getNotifier()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->notifier;
}


std::deque <chat_save_t> Sequencer::getChatHistory()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->chathistory;
}

std::vector<client_t> Sequencer::getClients()
{
    STACKLOG;
    Sequencer* instance = Instance();
	std::vector<client_t> res;
    MutexLocker scoped_lock(instance->clients_mutex);
	SWBaseSocket::SWBaseError error;

	for (unsigned int i = 0; i < instance->clients.size(); i++)
	{
		client_t c = *instance->clients[i];
		strcpy(c.ip_addr, instance->clients[i]->sock->get_peerAddr(&error).c_str());
		res.push_back(c);
	}
	return res;
}

int Sequencer::getStartTime()
{
    STACKLOG;
    Sequencer* instance = Instance();
	return instance->startTime;
}

client_t *Sequencer::getClient(int uid)
{
    STACKLOG;
	Sequencer* instance = Instance();

    unsigned short pos = instance->getPosfromUid(uid);
    if( UID_NOT_FOUND == pos ) return 0;

	return instance->clients[pos];
}

void Sequencer::updateMinuteStats()
{
    STACKLOG;
    Sequencer* instance = Instance();
	for (unsigned int i=0; i<instance->clients.size(); i++)
	{
		if (instance->clients[i]->status == USED)
		{
			for(std::map<unsigned int, stream_traffic_t>::iterator it = instance->clients[i]->streams_traffic.begin(); it!=instance->clients[i]->streams_traffic.end(); it++)
			{
				it->second.bandwidthIncomingRate = (it->second.bandwidthIncoming - it->second.bandwidthIncomingLastMinute)/60;
				it->second.bandwidthIncomingLastMinute = it->second.bandwidthIncoming;
				it->second.bandwidthOutgoingRate = (it->second.bandwidthOutgoing - it->second.bandwidthOutgoingLastMinute)/60;
				it->second.bandwidthOutgoingLastMinute = it->second.bandwidthOutgoing;
			}
		}
	}
}

// clients_mutex needs to be locked wen calling this method
void Sequencer::printStats()
{
    STACKLOG;
	if(!Config::getPrintStats()) return;
    Sequencer* instance = Instance();
	SWBaseSocket::SWBaseError error;
	{
		Logger::log(LOG_INFO, "Server occupancy:");

		Logger::log(LOG_INFO, "Slot Status   UID IP                  Colour, Nickname");
		Logger::log(LOG_INFO, "--------------------------------------------------");
		for (unsigned int i = 0; i < instance->clients.size(); i++)
		{
			// some auth identifiers
			char authst[10] = "";
			if(instance->clients[i]->user.authstatus & AUTH_ADMIN) strcat(authst, "A");
			if(instance->clients[i]->user.authstatus & AUTH_MOD) strcat(authst, "M");
			if(instance->clients[i]->user.authstatus & AUTH_RANKED) strcat(authst, "R");
			if(instance->clients[i]->user.authstatus & AUTH_BOT) strcat(authst, "B");
			if(instance->clients[i]->user.authstatus & AUTH_BANNED) strcat(authst, "X");

			// construct screen
			if (instance->clients[i]->status == FREE)
				Logger::log(LOG_INFO, "%4i Free", i);
			else if (instance->clients[i]->status == BUSY)
				Logger::log(LOG_INFO, "%4i Busy %5i %-16s % 4s %d, %s", i,
						instance->clients[i]->user.uniqueid, "-",
						authst,
						instance->clients[i]->user.colournum,
						instance->clients[i]->user.username);
			else
				Logger::log(LOG_INFO, "%4i Used %5i %-16s % 4s %d, %s", i,
						instance->clients[i]->user.uniqueid,
						instance->clients[i]->sock->get_peerAddr(&error).c_str(),
						authst,
						instance->clients[i]->user.colournum,
						instance->clients[i]->user.username);
		}
		Logger::log(LOG_INFO, "--------------------------------------------------");
		int timediff = Messaging::getTime() - instance->startTime;
		int uphours = timediff/60/60;
		int upminutes = (timediff-(uphours*60*60))/60;
		stream_traffic_t traffic = Messaging::getTraffic();

		Logger::log(LOG_INFO, "- traffic statistics (uptime: %d hours, %d "
				"minutes):", uphours, upminutes);
		Logger::log(LOG_INFO, "- total: incoming: %0.2fMB , outgoing: %0.2fMB",
				traffic.bandwidthIncoming/1024/1024,
				traffic.bandwidthOutgoing/1024/1024);
		Logger::log(LOG_INFO, "- rate (last minute): incoming: %0.1fkB/s , "
				"outgoing: %0.1fkB/s",
				traffic.bandwidthIncomingRate/1024,
				traffic.bandwidthOutgoingRate/1024);
	}
}
// used to access the clients from the array rather than using the array pos it's self.
unsigned short Sequencer::getPosfromUid(unsigned int uid)
{
    STACKLOG;
    Sequencer* instance = Instance();

    for (unsigned short i = 0; i < instance->clients.size(); i++)
    {
        if(instance->clients[i]->user.uniqueid == uid)
            return i;
    }

    Logger::log( LOG_DEBUG, "could not find uid %d", uid);
    return UID_NOT_FOUND;
}

void Sequencer::unregisterServer()
{
	if( Instance()->notifier )
		Instance()->notifier->unregisterServer();
}

