/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#include "sequencer.h"

#include "messaging.h"
#include "sha1_util.h"
#include "receiver.h"
#include "broadcaster.h"
#include "userauth.h"
#include "SocketW.h"
#include "logger.h"
#include "config.h"
#include "utils.h"
#include "ScriptEngine.h"

#include <stdio.h>
#include <time.h>
#include <chrono>
#include <string>
#include <iostream>
#include <stdexcept>
#include <sstream>

#ifdef __GNUC__

#include <stdlib.h>

#endif

Client::Client(Sequencer *sequencer, SWInetSocket *socket) :
        m_socket(socket),
        m_receiver(sequencer),
        m_broadcaster(sequencer),
        m_sequencer(sequencer),
        m_spamfilter(sequencer, this){
}

void Client::StartThreads() {
    m_receiver.Start(this);
    m_broadcaster.Start(this);
}

void Client::Disconnect() {
    // Signal threads to stop and wait for them to finish
    m_broadcaster.Stop();
    m_receiver.Stop();

    // Disconnect the socket
    SWBaseSocket::SWBaseError result;
    bool disconnected_ok = m_socket->disconnect(&result);
    if (!disconnected_ok || (result != SWBaseSocket::base_error::ok)) {
        Logger::Log(
                LOG_ERROR,
                "Internal: Error while disconnecting client - failed to disconnect socket. Message: %s",
                result.get_error().c_str());
    }
    delete m_socket;
}

bool Client::CheckSpawnRate()
{
    std::chrono::seconds spawn_interval(Config::getSpawnIntervalSec());
    if (spawn_interval.count() == 0 || Config::getMaxSpawnRate() == 0) {
        return true; // Spawn rate not limited
    }

    // Determine current rate
    int rate = 0;
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    auto itor = m_stream_reg_timestamps.begin();
    while (itor != m_stream_reg_timestamps.end()) {
        if (*itor < now - spawn_interval) {
            itor = m_stream_reg_timestamps.erase(itor);
        } else {
            ++rate;
            ++itor;
        }
    }

    // Evaluate current rate
    float rate_f = (float)rate / (float)Config::getMaxSpawnRate();
    if (rate_f > 0.7) {
        char msg[400];
        snprintf(msg, 400, "Do not spawn more than %d vehicles in %d seconds. Already spawned %d",
            Config::getMaxSpawnRate(), Config::getSpawnIntervalSec(), rate);
        m_sequencer->serverSay(msg, this->GetUserId(), FROM_SERVER);
    }

    // Add current time
    m_stream_reg_timestamps.push_back(std::chrono::system_clock::now());

    return rate <= Config::getMaxSpawnRate();
}

std::string Client::GetIpAddress() {
    SWBaseSocket::SWBaseError result;
    std::string ip = m_socket->get_peerAddr(&result);
    if (result != SWBaseSocket::base_error::ok) {
        Logger::Log(
                LOG_ERROR,
                "Internal: Error while getting client IP address. Message: %s",
                result.get_error().c_str());
    }
    return ip;
}

void Client::QueueMessage(int msg_type, int client_id, unsigned int stream_id, unsigned int payload_len,
                          const char *payload) {
    m_broadcaster.QueueMessage(msg_type, client_id, stream_id, payload_len, payload);
}

// Yes, this is weird. To be refactored.
void Client::NotifyAllVehicles(Sequencer *sequencer) {
    if (!m_is_initialized) {
        sequencer->IntroduceNewClientToAllVehicles(this);
        m_is_initialized = true;
    }
}

void *LaunchKillerThread(void *data) {
    Sequencer *sequencer = static_cast<Sequencer *>(data);
    sequencer->killerthreadstart();
    return nullptr;
}

Sequencer::Sequencer() :
        m_clients_mutex(/*recursive=*/ true),
        m_blacklist(this) {
    m_start_time = static_cast<int>(time(nullptr));
}

/**
 * Initialize, needs to be called before the class is used
 */
void Sequencer::Initialize() {
    m_clients.reserve(Config::getMaxClients());

#ifdef WITH_ANGELSCRIPT
    if (Config::getEnableScripting()) {
        m_script_engine = new ScriptEngine(this);
        m_script_engine->loadScript(Config::getScriptName());
    }
#endif //WITH_ANGELSCRIPT

    pthread_create(&m_killer_thread, NULL, LaunchKillerThread, this);

    m_auth_resolver = new UserAuth(Config::getAuthFile());

    m_blacklist.LoadBlacklistFromFile();
}

/**
 * Cleanup function is to be called when the Sequencer is done being used
 * this is in place of the destructor.
 */
void Sequencer::Close() {
    Logger::Log(LOG_INFO, "closing. disconnecting clients ...");

    const char *str = "server shutting down (try to reconnect later!)";
    for (unsigned int i = 0; i < m_clients.size(); i++) {
        // HACK-ISH override all thread stuff and directly send it!
        Client *client = m_clients[i];
        Messaging::SendMessage(client->GetSocket(), RoRnet::MSG2_USER_LEAVE, client->GetUserId(), 0, strlen(str),
                               str);
    }
    Logger::Log(LOG_INFO, "all clients disconnected. exiting.");

#ifdef WITH_ANGELSCRIPT
    if (m_script_engine != nullptr) {
        delete m_script_engine;
        m_script_engine = nullptr;
    }
#endif //WITH_ANGELSCRIPT

    if (m_auth_resolver != nullptr) {
        delete m_auth_resolver;
        m_auth_resolver = nullptr;
    }

    pthread_cancel(m_killer_thread);
    pthread_detach(m_killer_thread);
}

bool Sequencer::CheckNickIsUnique(std::string &nick) {
    // WARNING: be sure that this is only called within a clients_mutex lock!

    // check for duplicate names
    for (unsigned int i = 0; i < m_clients.size(); i++) {
        if (nick == m_clients[i]->GetUsername()) {
            return true;
        }
    }
    return false;
}


int Sequencer::GetFreePlayerColour() {
    // WARNING: be sure that this is only called within a clients_mutex lock!

    int col = 0;
    for (;;) // TODO: How many colors ARE there?
    {
        bool collision = false;
        for (unsigned int i = 0; i < m_clients.size(); i++) {
            if (m_clients[i]->user.colournum == col) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            return col;
        }
        col++;
    }
}

//this is called by the Listener thread
void Sequencer::createClient(SWInetSocket *sock, RoRnet::UserInfo user) {
    //we have a confirmed client that wants to play
    //try to find a place for him
    Logger::Log(LOG_DEBUG, "got instance in createClient()");

    MutexLocker scoped_lock(m_clients_mutex);

	std::string nick = Str::SanitizeUtf8(user.username);
    // check if banned
    SWBaseSocket::SWBaseError error;
    if (Sequencer::IsBanned(sock->get_peerAddr(&error).c_str())) {
        Logger::Log(LOG_WARN, "rejected banned client '%s' with IP %s", nick.c_str(), sock->get_peerAddr(&error).c_str());
        Messaging::SendMessage(sock, RoRnet::MSG2_BANNED, 0, 0, 0, 0);
        return;
    }

    // check if server is full
    Logger::Log(LOG_DEBUG, "searching free slot for new client...");
    if (m_clients.size() >= (Config::getMaxClients() + m_bot_count)) {
        Logger::Log(LOG_WARN, "join request from '%s' on full server: rejecting!",
                    Str::SanitizeUtf8(user.username).c_str());
        // set a low time out because we don't want to cause a back up of
        // connecting clients
        sock->set_timeout(10, 0);
        Messaging::SendMessage(sock, RoRnet::MSG2_FULL, 0, 0, 0, 0);
        throw std::runtime_error("Server is full");
    }

    if (nick.empty())
    {
        nick = "Anonymous";
        strncpy(user.username, nick.c_str(), RORNET_MAX_USERNAME_LEN - 1);
    }
    if (Sequencer::CheckNickIsUnique(nick)) {
        Logger::Log(LOG_WARN, std::string("found duplicate nick, getting new one: ") + nick);

        // shorten username so the number will fit (only if its too long already)
        std::string new_nick_base = nick.substr(0, RORNET_MAX_USERNAME_LEN - 4) + "-";

        for (int i = 2; i < 99; i++) {
            nick = new_nick_base + std::to_string(i);
            if (!Sequencer::CheckNickIsUnique(nick)) {
                Logger::Log(LOG_WARN, std::string("New username was composed: ") + nick);
                strncpy(user.username, nick.c_str(), RORNET_MAX_USERNAME_LEN - 1);
                break;
            }
        }
    }

    // Increase the botcount if this is a bot
    if ((user.authstatus & RoRnet::AUTH_BOT) > 0)
        m_bot_count++;

    //okay, create the client slot
    Client *to_add = new Client(this, sock);
    to_add->user = user;
    to_add->user.colournum = Sequencer::GetFreePlayerColour();
    to_add->user.authstatus = user.authstatus;
    std::string ip = to_add->GetIpAddress();

    // log some info about this client (in UTF8)
    char buf[3000];
    if (strlen(user.usertoken) > 0)
        sprintf(buf, "New client: %s (%s), with IP %s, using %s %s, with token %s", user.username, user.language, ip.data(), user.clientname,
                user.clientversion, std::string(user.usertoken, 40).c_str());
    else
        sprintf(buf, "New client: %s (%s), with IP %s, using %s %s, without token", user.username, user.language, ip.data(), user.clientname,
                user.clientversion);
    Logger::Log(LOG_INFO, Str::SanitizeUtf8(buf));

    // assign unique userid
    unsigned int client_id = m_free_user_id;
    to_add->user.uniqueid = client_id;

    // count up unique id
    m_free_user_id++;

    // add the client to the vector
    m_clients.push_back(to_add);
    // create one thread for the receiver
    // and one for the broadcaster
    to_add->StartThreads();

    Logger::Log(LOG_VERBOSE, "Sending welcome message to uid %u", client_id);
    if (Messaging::SendMessage(sock, RoRnet::MSG2_WELCOME, client_id, 0, sizeof(RoRnet::UserInfo),
                               (char *) &to_add->user)) {
        this->QueueClientForDisconnect(client_id, "error sending welcome message");
        return;
    }

    // Do script callback
#ifdef WITH_ANGELSCRIPT
    if (m_script_engine != nullptr) {
        m_script_engine->playerAdded(client_id);
    }
#endif //WITH_ANGELSCRIPT

    // notify everyone of the new client
    // but blank out the user token and GUID
    RoRnet::UserInfo info_for_others = to_add->user;
    memset(info_for_others.usertoken, 0, 40);
    memset(info_for_others.clientGUID, 0, 40);
    for (unsigned int i = 0; i < m_clients.size(); i++) {
        m_clients[i]->QueueMessage(RoRnet::MSG2_USER_JOIN, (int)client_id, 0, sizeof(RoRnet::UserInfo),
                                   (char *) &info_for_others);
    }

    printStats();

    // done!
    Logger::Log(LOG_VERBOSE, "Sequencer: New client added");
}

// Only used by scripting
void Sequencer::broadcastUserInfo(int client_id) {
    Client *client = this->FindClientById(static_cast<unsigned int>(client_id));
    if (client == nullptr) {
        return;
    }

    // notify everyone of the client
    // but blank out the user token and GUID
    RoRnet::UserInfo info_for_others = client->user;
    memset(info_for_others.usertoken, 0, 40);
    memset(info_for_others.clientGUID, 0, 40);
    { // Lock scope
        MutexLocker scoped_lock(m_clients_mutex);
        for (unsigned int i = 0; i < m_clients.size(); i++) {
            m_clients[i]->QueueMessage(RoRnet::MSG2_USER_INFO, info_for_others.uniqueid, 0, sizeof(RoRnet::UserInfo),
                                       (char *) &info_for_others);
        }
    }
}

void Sequencer::GetHeartbeatUserList(Json::Value &out_array) {
    MutexLocker scoped_lock(m_clients_mutex);

    auto itor = m_clients.begin();
    auto endi = m_clients.end();
    for (; itor != endi; ++itor) {
        Client *client = *itor;
        Json::Value user_data(Json::objectValue);
        user_data["is_admin"] = (client->user.authstatus & RoRnet::AUTH_ADMIN);
        user_data["is_mod"] = (client->user.authstatus & RoRnet::AUTH_MOD);
        user_data["is_ranked"] = (client->user.authstatus & RoRnet::AUTH_RANKED);
        user_data["is_bot"] = (client->user.authstatus & RoRnet::AUTH_BOT);
        user_data["username"] = client->user.username;
        user_data["ip_address"] = client->GetIpAddress();
        user_data["client_id"] = client->GetUserId();

        out_array.append(user_data);
    }
}

int Sequencer::getNumClients() {
    MutexLocker scoped_lock(m_clients_mutex);
    return (int) m_clients.size();
}

int Sequencer::AuthorizeNick(std::string token, std::string &nickname) {
    MutexLocker scoped_lock(m_clients_mutex);
    if (m_auth_resolver == nullptr) {
        return RoRnet::AUTH_NONE;
    }
    return m_auth_resolver->resolve(token, nickname, m_free_user_id);
}

void Sequencer::killerthreadstart() {
    Logger::Log(LOG_DEBUG, "Killer thread ready");
    while (1) {
        Logger::Log(LOG_DEBUG, "Killer entering cycle");

        m_killer_mutex.lock();
        while (m_kill_queue.empty()) {
            m_killer_mutex.wait(m_killer_cond);
        }

        //pop the kill queue
        Client *to_del = m_kill_queue.front();
        m_kill_queue.pop();
        m_killer_mutex.unlock();

        Logger::Log(LOG_DEBUG, "Killer called to kill %s", to_del->GetUsername().c_str());
        to_del->Disconnect();

        delete to_del;
        to_del = NULL;
    }
}

void Sequencer::QueueClientForDisconnect(int uid, const char *errormsg, bool isError /*=true*/, bool doScriptCallback /*= true*/) {
    MutexLocker scoped_lock(m_clients_mutex);

    Client *client = this->FindClientById(static_cast<unsigned int>(uid));
    if (client == nullptr) {
        Logger::Log(LOG_DEBUG,
            "Sequencer::QueueClientForDisconnect() Internal error, got non-existent user ID: %d"
            "(error message: '%s')", uid, errormsg);
        return;
    }

    // send an event if user is rankend and if we are a official server
    /* Disabled until new multiplayer portal supports it.
    if (m_auth_resolver && (client->user.authstatus & RoRnet::AUTH_RANKED)) {
        m_auth_resolver->sendUserEvent(std::string(client->user.usertoken, 40), (isError ? "crash" : "leave"),
                                       Str::SanitizeUtf8(client->user.username), "");
    }
    */

#ifdef WITH_ANGELSCRIPT
    if (m_script_engine != nullptr && doScriptCallback) {
        m_script_engine->playerDeleted(client->GetUserId(), isError ? 1 : 0);
    }
#endif //WITH_ANGELSCRIPT

    // Update the botCount value
    if ((client->user.authstatus & RoRnet::AUTH_BOT) > 0) {
        m_bot_count--;
    }

    //notify the others
    int pos = 0;
    for (unsigned int i = 0; i < m_clients.size(); i++) {
        m_clients[i]->QueueMessage(RoRnet::MSG2_USER_LEAVE, uid, 0, (int) strlen(errormsg), errormsg);
        if (m_clients[i]->GetUserId() == uid) {
            pos = i;
        }
    }
    m_clients.erase(m_clients.begin() + pos);

    printStats();

    //this routine is a potential trouble maker as it can be called from many thread contexts
    //so we use a killer thread
    Logger::Log(LOG_VERBOSE, "Disconnecting client ID %d: %s", uid, errormsg);
    Logger::Log(LOG_DEBUG, "adding client to kill queue, size: %zu", m_kill_queue.size());
    {
        MutexLocker scoped_lock(m_killer_mutex);
        m_kill_queue.push(client);
    }
    m_killer_cond.signal();

    m_num_disconnects_total++;
    if (isError) {
        m_num_disconnects_crash++;
    }
    Logger::Log(LOG_INFO, "crash statistic: %zu of %zu deletes crashed",
        m_num_disconnects_crash, m_num_disconnects_total);
}

//this is called from the listener thread initial handshake
void Sequencer::enableFlow(int uid) {
    MutexLocker scoped_lock(m_clients_mutex);

    Client *client = this->FindClientById(static_cast<unsigned int>(uid));
    if (client == nullptr) {
        return;
    }

    client->SetReceiveData(true);
}


//this is called from the listener thread initial handshake
int Sequencer::sendMOTD(int uid) {
    std::vector<std::string> lines;
    int res = Utils::ReadLinesFromFile(Config::getMOTDFile(), lines);
    if (res)
        return res;

    std::vector<std::string>::iterator it;
    for (it = lines.begin(); it != lines.end(); it++) {
        serverSay(*it, uid, FROM_MOTD);
    }
    return 0;
}

UserAuth *Sequencer::getUserAuth() {
    return m_auth_resolver;
}

//this is called from the listener thread initial handshake
void Sequencer::IntroduceNewClientToAllVehicles(Client *new_client) {
    RoRnet::UserInfo info_for_others = new_client->user;
    memset(info_for_others.usertoken, 0, 40);
    memset(info_for_others.clientGUID, 0, 40);

    for (unsigned int i = 0; i < m_clients.size(); i++) {
        Client *client = m_clients[i];
        if (client->GetStatus() == Client::STATUS_USED) {
            // new user to all others
            client->QueueMessage(RoRnet::MSG2_USER_INFO, new_client->GetUserId(), 0, sizeof(RoRnet::UserInfo),
                                 (char *) &info_for_others);

            // all others to new user
            RoRnet::UserInfo info_for_newcomer = m_clients[i]->user;
            memset(info_for_newcomer.usertoken, 0, 40);
            memset(info_for_newcomer.clientGUID, 0, 40);
            new_client->QueueMessage(RoRnet::MSG2_USER_INFO, client->GetUserId(), 0, sizeof(RoRnet::UserInfo),
                                     (char *) &info_for_newcomer);

            Logger::Log(LOG_VERBOSE, " * %zu streams registered for user %d", m_clients[i]->streams.size(),
                        m_clients[i]->GetUserId());

            auto itor = client->streams.begin();
            auto endi = client->streams.end();
            for (; itor != endi; ++itor) {
                Logger::Log(LOG_VERBOSE, "sending stream registration %d:%u to user %d", client->GetUserId(),
                            itor->first, new_client->GetUserId());
                new_client->QueueMessage(RoRnet::MSG2_STREAM_REGISTER, client->GetUserId(), itor->first,
                                         sizeof(RoRnet::StreamRegister), (char *) &itor->second);
            }
        }
    }
}

int Sequencer::sendGameCommand(int uid, std::string cmd) {
    const char *data = cmd.c_str();
    int size = cmd.size();

    if (uid == TO_ALL) {
        for (int i = 0; i < (int) m_clients.size(); i++) {
            m_clients[i]->QueueMessage(RoRnet::MSG2_GAME_CMD, -1, 0, size, data);
        }
    } else {
        Client *client = this->FindClientById(static_cast<unsigned int>(uid));
        if (client != nullptr) {
            client->QueueMessage(RoRnet::MSG2_GAME_CMD, -1, 0, size, data); // ClientId -1: comes from the server
        }
    }
    return 0;
}

// this does not lock the clients_mutex, make sure it is locked before hand
// note: uid==-1==TO_ALL = broadcast your message to all players
void Sequencer::serverSay(std::string msg, int uid, int type) {
    switch (type) {
        case FROM_SERVER:
            msg = std::string("SERVER: ") + msg;
            break;

        case FROM_HOST:
            if (uid == -1) {
                msg = std::string("Host(general): ") + msg;
            } else {
                msg = std::string("Host(private): ") + msg;
            }
            break;

        case FROM_RULES:
            msg = std::string("Rules: ") + msg;
            break;

        case FROM_MOTD:
            msg = std::string("MOTD: ") + msg;
            break;
    }

    std::string msg_valid = Str::SanitizeUtf8(msg.begin(), msg.end());
    auto itor = m_clients.begin();
    auto endi = m_clients.end();
    for (; itor != endi; ++itor) {
        Client *client = *itor;
        if ((client->GetStatus() == Client::STATUS_USED) &&
            client->IsReceivingData() &&
            (uid == TO_ALL || ((int) client->GetUserId()) == uid)) {
            client->QueueMessage(RoRnet::MSG2_UTF8_CHAT, -1, -1, msg_valid.length(), msg_valid.c_str());
        }
    }
}

void Sequencer::serverSayThreadSave(std::string msg, int uid, int type) {
    this->serverSay(msg, uid, type);
}

bool Sequencer::Kick(int kuid, int modUID, const char *msg) {
    Client *kicked_client = this->FindClientById(static_cast<unsigned int>(kuid));
    if (kicked_client == nullptr) {
        return false;
    }
    Client *mod_client = this->FindClientById(static_cast<unsigned int>(modUID));
    if (mod_client == nullptr) {
        return false;
    }

    char kickmsg[1024] = "";
    strcat(kickmsg, "kicked by ");
    strcat(kickmsg, Str::SanitizeUtf8(mod_client->user.username).c_str());
    if (msg) {
        strcat(kickmsg, " for ");
        strcat(kickmsg, msg);
    }

    char kickmsg2[1036] = "";
    sprintf(kickmsg2, "player %s was %s", Str::SanitizeUtf8(kicked_client->user.username).c_str(), kickmsg);
    serverSay(kickmsg2, TO_ALL, FROM_SERVER);

    Logger::Log(
            LOG_VERBOSE,
            "player '%s' kicked by '%s'",
            Str::SanitizeUtf8(kicked_client->user.username).c_str(),
            Str::SanitizeUtf8(mod_client->user.username).c_str());

    this->QueueClientForDisconnect(kicked_client->GetUserId(), kickmsg, false);
    return true;
}

void Sequencer::RecordBan(int bid,
	std::string const& ip_addr,
    std::string const& nickname,
    std::string const& by_nickname,
    std::string const& banmsg)
{
    // construct ban data and add it to the list
    ban_t *b = new ban_t;
    memset(b, 0, sizeof(ban_t));

	// a somewhat auto-incrementing id based on the vector size
	if (bid != -1) {
		b->bid = bid;
	} else {
		b->bid = m_bans.size();
	}
    strncpy(b->banmsg, banmsg.c_str(), /* copy max: */255);
    strncpy(b->ip, ip_addr.c_str(), 16);
    strncpy(b->nickname, nickname.c_str(), /* copy max: */RORNET_MAX_USERNAME_LEN - 1);
    strncpy(b->bannedby_nick, by_nickname.c_str(), /* copy max: */RORNET_MAX_USERNAME_LEN - 1);

    Logger::Log(LOG_DEBUG, "adding ban, size: %u", m_bans.size());
    m_bans.push_back(b);
    Logger::Log(LOG_VERBOSE, "new ban added: '%s' by '%s'", nickname.c_str(), by_nickname.c_str());
}

bool Sequencer::Ban(int buid, int modUID, const char *msg) {
    Client *banned_client = this->FindClientById(static_cast<unsigned int>(buid));
    if (banned_client == nullptr) {
        return false;
    }
    Client *mod_client = this->FindClientById(static_cast<unsigned int>(modUID));
    if (mod_client == nullptr) {
        return false;
    }

    this->RecordBan(-1, banned_client->GetIpAddress(),
                    banned_client->GetUsername(), mod_client->GetUsername(), msg);
    m_blacklist.SaveBlacklistToFile(); // Persist the ban

    std::string kick_msg = msg + std::string(" (banned)");
    return Kick(buid, modUID, kick_msg.c_str());
}

void Sequencer::SilentBan(int buid, const char *msg, bool doScriptCallback /*= true*/) {
    Client *banned_client = this->FindClientById(static_cast<unsigned int>(buid));
    if (banned_client == nullptr) {
        Logger::Log(LOG_ERROR, "void Sequencer::ban(%d, %s) --> uid %d not found!", buid, msg, buid);
        return;
    }

    this->RecordBan(-1, banned_client->GetIpAddress(),
        banned_client->GetUsername(), "rorserver", msg);
    m_blacklist.SaveBlacklistToFile(); // Persist the ban

    std::string kick_msg = msg + std::string(" (banned)");
    QueueClientForDisconnect(banned_client->GetUserId(), kick_msg.c_str(), false, doScriptCallback);
}

bool Sequencer::UnBan(int bid) {
    for (unsigned int i = 0; i < m_bans.size(); i++) {
        if (m_bans[i]->bid == (unsigned int)bid) {
            m_bans.erase(m_bans.begin() + i);
			m_blacklist.SaveBlacklistToFile(); // Remove from the blacklist file
            Logger::Log(LOG_VERBOSE, "ban removed: %d", bid);
            return true;
        }
    }
    return false;
}

bool Sequencer::IsBanned(const char *ip) {
    if (ip == nullptr) {
        return false;
    }

    for (unsigned int i = 0; i < m_bans.size(); i++) {
        if (!strcmp(m_bans[i]->ip, ip))
            return true;
    }
    return false;
}

void Sequencer::streamDebug() {
    for (unsigned int i = 0; i < m_clients.size(); i++) {
        if (m_clients[i]->GetStatus() == Client::STATUS_USED) {
            Logger::Log(LOG_VERBOSE, " * %d %s (slot %d):", m_clients[i]->GetUserId(),
                        m_clients[i]->GetUsername().c_str(), i);
            if (!m_clients[i]->streams.size())
                Logger::Log(LOG_VERBOSE, "  * no streams registered for user %d", m_clients[i]->GetUserId());
            else
                for (std::map<unsigned int, RoRnet::StreamRegister>::iterator it = m_clients[i]->streams.begin();
                     it != m_clients[i]->streams.end(); it++) {
                    char *types[] = {(char *) "truck", (char *) "character", (char *) "aitraffic", (char *) "chat"};
                    char *typeStr = (char *) "unkown";
                    if (it->second.type >= 0 && it->second.type <= 3)
                        typeStr = types[it->second.type];
                    Logger::Log(LOG_VERBOSE, "  * %d:%u, type:%s status:%d name:'%s'", m_clients[i]->GetUserId(),
                                it->first, typeStr, it->second.status, it->second.name);
                }
        }
    }
}

//this is called by the receivers threads, like crazy & concurrently
void Sequencer::queueMessage(int uid, int type, unsigned int streamid, char *data, unsigned int len) {
    MutexLocker scoped_lock(m_clients_mutex);

    Client *client = this->FindClientById(static_cast<unsigned int>(uid));
    if (client == nullptr) {
        return;
    }

    // check for full broadcaster queue
    {
        bool is_dropping = client->IsBroadcasterDroppingPackets();
        if (is_dropping && client->drop_state == 0) {
            // queue full, inform client
            int drop_state = 1;
            client->drop_state = drop_state;
            client->QueueMessage(RoRnet::MSG2_NETQUALITY, -1, 0, sizeof(int), (char *) &drop_state);
        } else if (!is_dropping && client->drop_state == 1) {
            // queue working better again, inform client
            int drop_state = 0;
            client->drop_state = drop_state;
            client->QueueMessage(RoRnet::MSG2_NETQUALITY, -1, 0, sizeof(int), (char *) &drop_state);
        }
    }

    broadcastType publishMode = BROADCAST_BLOCK;

    switch (type)
    {
    case RoRnet::MSG2_STREAM_DATA:
    case RoRnet::MSG2_STREAM_DATA_DISCARDABLE:
        publishMode = this->ProcessStreamData(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_STREAM_REGISTER:
        publishMode = this->ProcessStreamRegister(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_STREAM_REGISTER_RESULT:
        publishMode = this->ProcessStreamRegisterResult(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_STREAM_UNREGISTER:
        publishMode = this->ProcessStreamUnRegister(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_USER_LEAVE:
        publishMode = this->ProcessUserLeave(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_UTF8_CHAT:
        publishMode = this->ProcessUtf8Chat(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_UTF8_PRIVCHAT:
        publishMode = this->ProcessUtf8PrivChat(client, type, streamid, data, len);
        break;

    case RoRnet::MSG2_GAME_CMD:
        publishMode = this->ProcessGameCmd(client, type, streamid, data, len);

    default:;
    }

    if (publishMode < BROADCAST_BLOCK) {
        client->streams_traffic[streamid].bandwidthIncoming += len;

        if (publishMode == BROADCAST_NORMAL || publishMode == BROADCAST_ALL) {
            bool toAll = (publishMode == BROADCAST_ALL);
            // just push to all the present clients
            for (unsigned int i = 0; i < m_clients.size(); i++) {
                Client *curr_client = m_clients[i];
                if (curr_client->GetStatus() == Client::STATUS_USED && curr_client->IsReceivingData() &&
                    (curr_client != client || toAll)) {
                    curr_client->streams_traffic[streamid].bandwidthOutgoing += len;
                    curr_client->QueueMessage(type, client->GetUserId(), streamid, len, data);
                }
            }
        } else if (publishMode == BROADCAST_AUTHED) {
            // push to all bots and authed users above auth level 1
            for (unsigned int i = 0; i < m_clients.size(); i++) {
                Client *curr_client = m_clients[i];
                if (curr_client->GetStatus() == Client::STATUS_USED && curr_client->IsReceivingData() &&
                    (curr_client != client) && (client->user.authstatus & RoRnet::AUTH_ADMIN)) {
                    curr_client->streams_traffic[streamid].bandwidthOutgoing += len;
                    curr_client->QueueMessage(type, client->GetUserId(), streamid, len, data);
                }
            }
        }
    }
}

int Sequencer::getStartTime() {
    return m_start_time;
}

Client *Sequencer::getClient(int uid) {
    return this->FindClientById(static_cast<unsigned int>(uid));
}

void Sequencer::UpdateMinuteStats() {
    MutexLocker scoped_lock(m_clients_mutex);

    for (unsigned int i = 0; i < m_clients.size(); i++) {
        if (m_clients[i]->GetStatus() == Client::STATUS_USED) {
            for (std::map<unsigned int, stream_traffic_t>::iterator it = m_clients[i]->streams_traffic.begin();
                 it != m_clients[i]->streams_traffic.end(); it++) {
                it->second.bandwidthIncomingRate =
                        (it->second.bandwidthIncoming - it->second.bandwidthIncomingLastMinute) / 60;
                it->second.bandwidthIncomingLastMinute = it->second.bandwidthIncoming;
                it->second.bandwidthOutgoingRate =
                        (it->second.bandwidthOutgoing - it->second.bandwidthOutgoingLastMinute) / 60;
                it->second.bandwidthOutgoingLastMinute = it->second.bandwidthOutgoing;
            }
        }
    }
}

// clients_mutex needs to be locked wen calling this method
void Sequencer::printStats() {
    if (!Config::getPrintStats()) {
        return;
    }

    {
        Logger::Log(LOG_INFO, "Server occupancy:");

        Logger::Log(LOG_INFO, "Slot Status   UID IP                  Colour, Nickname");
        Logger::Log(LOG_INFO, "--------------------------------------------------");
        for (unsigned int i = 0; i < m_clients.size(); i++) {
            // some auth identifiers
            char authst[10] = "";
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_ADMIN) strcat(authst, "A");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_MOD) strcat(authst, "M");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_RANKED) strcat(authst, "R");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_BOT) strcat(authst, "B");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_BANNED) strcat(authst, "X");

            // construct screen
            if (m_clients[i]->GetStatus() == Client::STATUS_FREE)
                Logger::Log(LOG_INFO, "%4u Free", i);
            else if (m_clients[i]->GetStatus() == Client::STATUS_BUSY)
                Logger::Log(LOG_INFO, "%4u Busy %5i %-16s % 4s %d, %s", i,
                            m_clients[i]->GetUserId(), "-",
                            authst,
                            m_clients[i]->user.colournum,
                            m_clients[i]->GetUsername().c_str());
            else
                Logger::Log(LOG_INFO, "%4u Used %5i %-16s % 4s %d, %s", i,
                            m_clients[i]->GetUserId(),
                            m_clients[i]->GetIpAddress().c_str(),
                            authst,
                            m_clients[i]->user.colournum,
                            m_clients[i]->GetUsername().c_str());
        }
        Logger::Log(LOG_INFO, "--------------------------------------------------");
        int timediff = Messaging::getTime() - m_start_time;
        int uphours = timediff / 60 / 60;
        int upminutes = (timediff - (uphours * 60 * 60)) / 60;
        stream_traffic_t traffic = Messaging::GetTrafficStats();

        Logger::Log(LOG_INFO, "- traffic statistics (uptime: %d hours, %d "
                "minutes):", uphours, upminutes);
        Logger::Log(LOG_INFO, "- total: incoming: %0.2fMB , outgoing: %0.2fMB",
                    traffic.bandwidthIncoming / 1024 / 1024,
                    traffic.bandwidthOutgoing / 1024 / 1024);
        Logger::Log(LOG_INFO, "- rate (last minute): incoming: %0.1fkB/s , "
                            "outgoing: %0.1fkB/s",
                    traffic.bandwidthIncomingRate / 1024,
                    traffic.bandwidthOutgoingRate / 1024);
    }
}

// clients_mutex needs to be locked wen calling this method
// Invoked either from Sequencer or ServerScript
Client *Sequencer::FindClientById(unsigned int client_id) {
    auto itor = m_clients.begin();
    auto endi = m_clients.end();
    for (; itor != endi; ++itor) {
        Client *client = *itor;
        if (client->GetUserId() == (int)client_id) {
            return client;
        }
    }
    return nullptr;
}

std::vector<WebserverClientInfo> Sequencer::GetClientListCopy() {
    MutexLocker scoped_lock(m_clients_mutex);

    std::vector<WebserverClientInfo> output;
    for (Client *c : m_clients) {
        output.push_back(c);
    }
    return output;
}

std::vector<ban_t> Sequencer::GetBanListCopy()
{
    // FIXME: bans really should be synchronized ~ 09/2019

    std::vector<ban_t> output;
    for (ban_t *b : m_bans) {
        output.push_back(*b);
    }
    return output;
}

    // -------------------------------------------------- //
    //              RoRnet message handling               //
    // -------------------------------------------------- //

broadcastType Sequencer::ProcessStreamData(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    client->NotifyAllVehicles(this);

    broadcastType publishMode = BROADCAST_NORMAL;

    // Simple data validation (needed due to bug in RoR 0.38)
    {
        std::map<unsigned int, RoRnet::StreamRegister>::iterator it = client->streams.find(streamid);
        if (it == client->streams.end())
            publishMode = BROADCAST_BLOCK;
    }

    return publishMode;
}

broadcastType Sequencer::ProcessStreamRegister(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    broadcastType publishMode = BROADCAST_BLOCK;

    RoRnet::StreamRegister *reg = (RoRnet::StreamRegister *) data;
    if (client->streams.size() >= Config::getMaxVehicles() + NON_VEHICLE_STREAMS) {
        // This user has too many vehicles, we drop the stream and then disconnect the user
        Logger::Log(LOG_INFO, "%s(%d) has too many streams. Stream dropped, user kicked.",
                    client->GetUsername().c_str(), client->GetUserId());

        // send a message to the user.
        serverSay("You are now being kicked for having too many vehicles. Please rejoin.", client->GetUserId(),
                    FROM_SERVER);

        // broadcast a general message that this user was auto-kicked
        char sayMsg[128] = "";
        sprintf(sayMsg, "%s was auto-kicked for having too many vehicles (limit: %u)",
                client->GetUsername().c_str(), Config::getMaxVehicles());
        serverSay(sayMsg, TO_ALL, FROM_SERVER);

        QueueClientForDisconnect(client->GetUserId(), "You have too many vehicles. Please rejoin.", false);
        publishMode = BROADCAST_BLOCK; // drop
    } else if (reg->type == STREAM_REG_TYPE_VEHICLE && !client->CheckSpawnRate()) {
        // This user spawns vehicles too fast, we drop the stream and then disconnect the user
        Logger::Log(LOG_INFO, "%s(%d) spawns vehicles too fast. Stream dropped, user kicked.",
                    client->GetUsername().c_str(), client->GetUserId());

        // broadcast a general message that this user was auto-kicked
        char sayMsg[300] = "";
        snprintf(sayMsg, 300, "%s was auto-kicked for spawning vehicles too fast (limit: %d spawns per %d sec)",
                client->GetUsername().c_str(), Config::getMaxSpawnRate(), Config::getSpawnIntervalSec());
        serverSay(sayMsg, TO_ALL, FROM_SERVER);

        // disconnect the user with a message
        snprintf(sayMsg, 300, "You were auto-kicked for spawning vehicles too fast (limit: %d spawns per %d sec). Please rejoin.",
                Config::getMaxSpawnRate(), Config::getSpawnIntervalSec());
        QueueClientForDisconnect(client->GetUserId(), sayMsg, false);
        publishMode = BROADCAST_BLOCK; // drop
    } else if (reg->type == STREAM_REG_TYPE_VEHICLE && !Utils::isValidVehicleFileName(reg->name)) {
        // This user spawned vehicle with empty or malformed name, we drop the stream and then disconnect the user
        Logger::Log(LOG_INFO, "%s(%d) spawned vehicle with empty/malformed name. Stream dropped, user kicked.",
                    client->GetUsername().c_str(), client->GetUserId());

        // broadcast a general message that this user was auto-kicked
        char sayMsg[300] = "";
        snprintf(sayMsg, 300, "%s was auto-kicked for spawning vehicle with empty/malformed name", client->GetUsername().c_str());
        serverSay(sayMsg, TO_ALL, FROM_SERVER);

        // disconnect the user with a message
        snprintf(sayMsg, 300, "You were auto-kicked for spawning invalid vehicle");

        QueueClientForDisconnect(client->GetUserId(), sayMsg, false);
        publishMode = BROADCAST_BLOCK; // drop
    } else {
        publishMode = BROADCAST_NORMAL;

#ifdef WITH_ANGELSCRIPT
        // Do a script callback
        if (m_script_engine) {
            int scriptpub = m_script_engine->streamAdded(client->GetUserId(), reg);

            // We only support blocking and normal at the moment. Other modes are not supported.
            switch (scriptpub) {
                case BROADCAST_AUTO:
                    break;

                case BROADCAST_BLOCK:
                    publishMode = BROADCAST_BLOCK;
                    break;

                case BROADCAST_NORMAL:
                    publishMode = BROADCAST_NORMAL;
                    break;

                default:
                    Logger::Log(LOG_ERROR, "Stream broadcasting mode not supported.");
                    break;
            }
        }
#endif //WITH_ANGELSCRIPT

        if (publishMode != BROADCAST_BLOCK) {
            // Add the stream
            reg->name[127] = 0;
            Logger::Log(LOG_VERBOSE, " * new stream registered: %d:%u, type: %d, name: '%s', status: %d",
                        client->GetUserId(), streamid, reg->type, reg->name, reg->status);
            client->streams[streamid] = *reg;

            // send an event if user is rankend and if we are a official server
            if (m_auth_resolver && (client->user.authstatus & RoRnet::AUTH_RANKED))
                m_auth_resolver->sendUserEvent(std::string(client->user.usertoken, 40), std::string("newvehicle"),
                                                std::string(reg->name), std::string());

            // Notify the user about the vehicle limit
            if ((client->streams.size() >= Config::getMaxVehicles() + NON_VEHICLE_STREAMS - 3) &&
                (client->streams.size() > NON_VEHICLE_STREAMS)) {
                // we start warning the user as soon as he has only 3 vehicles left before he will get kicked (that's why we do minus three in the 'if' statement above).
                char sayMsg[128] = "";

                // special case if the user has exactly 1 vehicle
                if (client->streams.size() == NON_VEHICLE_STREAMS + 1)
                    sprintf(sayMsg, "You now have 1 vehicle. The vehicle limit on this server is set to %u.",
                            Config::getMaxVehicles());
                else
                    sprintf(sayMsg, "You now have %lu vehicles. The vehicle limit on this server is set to %u.",
                            (client->streams.size() - NON_VEHICLE_STREAMS), Config::getMaxVehicles());

                serverSay(sayMsg, client->GetUserId(), FROM_SERVER);
            }

            this->streamDebug();

            // reset some stats
            // streams_traffic limited through streams map
            client->streams_traffic[streamid].bandwidthIncoming = 0;
            client->streams_traffic[streamid].bandwidthIncomingLastMinute = 0;
            client->streams_traffic[streamid].bandwidthIncomingRate = 0;
            client->streams_traffic[streamid].bandwidthOutgoing = 0;
            client->streams_traffic[streamid].bandwidthOutgoingLastMinute = 0;
            client->streams_traffic[streamid].bandwidthOutgoingRate = 0;
        }
    }

    return publishMode;
}

broadcastType Sequencer::ProcessStreamRegisterResult(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    // forward message to the stream origin
    RoRnet::StreamRegister *reg = (RoRnet::StreamRegister *) data;
    Client *origin_client = this->FindClientById(reg->origin_sourceid);
    if (origin_client != nullptr) {
        origin_client->QueueMessage(type, client->GetUserId(), streamid, sizeof(RoRnet::StreamRegister), (char *) reg);
        Logger::Log(LOG_VERBOSE, "stream registration result for stream %03d:%03d from user %03d: %d",
                    reg->origin_sourceid, reg->origin_streamid, client->GetUserId(), reg->status);
    }
    return BROADCAST_BLOCK;
}

broadcastType Sequencer::ProcessStreamUnRegister(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    if (client->streams.erase(streamid) > 0) {
        Logger::Log(LOG_VERBOSE, " * stream deregistered: %d:%u", client->GetUserId(), streamid);
        return BROADCAST_ALL;
    } else {
        return BROADCAST_BLOCK;
    }
}

broadcastType Sequencer::ProcessUserLeave(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    // from client
    Logger::Log(LOG_INFO, "User disconnects on request: " + client->GetUsername());
    this->QueueClientForDisconnect(client->GetUserId(), "disconnected on request", false);
    return BROADCAST_BLOCK;
}

broadcastType Sequencer::ProcessUtf8Chat(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    std::string str = Str::SanitizeUtf8(data);
    Logger::Log(LOG_INFO, "CHAT| %s: %s", client->GetUsername(), str.c_str());

    broadcastType publishMode = BROADCAST_ALL;
    if (str[0] == '!') {
        publishMode = BROADCAST_BLOCK; // no broadcast of server commands!
    } else if (SpamFilter::IsActive() &&
                client->GetSpamFilter().CheckForSpam(str)) {
        publishMode = BROADCAST_BLOCK; // Client is gagged
    }

#ifdef WITH_ANGELSCRIPT
    if (m_script_engine) {
        int scriptpub = m_script_engine->playerChat(client->GetUserId(), str);
        if (scriptpub != BROADCAST_AUTO) publishMode = scriptpub;
    }
#endif //WITH_ANGELSCRIPT
    if (str == "!help") {
        serverSay(std::string("builtin commands:"), client->GetUserId());
        serverSay(std::string("!version, !list, !say, !bans, !ban, !unban, !kick, !vehiclelimit"), client->GetUserId());
        serverSay(std::string("!website, !irc, !owner, !voip, !rules, !motd"), client->GetUserId());
    }

    if (str == "!version") {
        std::string ver (VERSION);
        ver += " ";
        ver += RORNET_VERSION;
        serverSay(std::string(ver), client->GetUserId());
    } else if (str == "!list") {
        serverSay(std::string(" client->GetUserId() | auth   | nick"), client->GetUserId());
        for (unsigned int i = 0; i < m_clients.size(); i++) {
            if (i >= m_clients.size())
                break;
            char authst[10] = "";
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_ADMIN) strcat(authst, "A");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_MOD) strcat(authst, "M");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_RANKED) strcat(authst, "R");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_BOT) strcat(authst, "B");
            if (m_clients[i]->user.authstatus & RoRnet::AUTH_BANNED) strcat(authst, "X");\

            char tmp2[256] = "";
            sprintf(tmp2, "% 3d | %-6s | %-20s", m_clients[i]->GetUserId(), authst, client->GetUsername().c_str());
            serverSay(std::string(tmp2), client->GetUserId());
        }
    } else if (str.substr(0, 5) == "!bans") {
        if (client->user.authstatus & RoRnet::AUTH_MOD || client->user.authstatus & RoRnet::AUTH_ADMIN) {
            serverSay(std::string("id | IP              | nickname             | banned by"), client->GetUserId());
            if (m_bans.empty()) {
                serverSay(std::string("There are no bans recorded!"), client->GetUserId());
            } else {
                for (unsigned int i = 0; i < m_bans.size(); i++) {
                    char tmp[256] = "";
                    sprintf(tmp, "%3u | %-15s | %-20s | %-20s",
                        m_bans[i]->bid,
                        m_bans[i]->ip,
                        m_bans[i]->nickname,
                        m_bans[i]->bannedby_nick);
                    serverSay(std::string(tmp), client->GetUserId());
                }
            }
        } else {
            // not allowed
            serverSay(std::string("You are not authorized to use this command!"), client->GetUserId());
        }
    } else if (str.substr(0, 7) == "!unban ") {
        if (client->user.authstatus & RoRnet::AUTH_MOD || client->user.authstatus & RoRnet::AUTH_ADMIN) {
            int bid = -1;
            int res = sscanf(str.substr(7).c_str(), "%d", &bid);
            if (res != 1 || bid == -1) {
                serverSay(std::string("usage: !unban <bid>"), client->GetUserId());
                serverSay(std::string("example: !unban 3"), client->GetUserId());
            } else {
                if (UnBan(bid))
                    serverSay(std::string("ban removed"), client->GetUserId());
                else
                    serverSay(std::string("ban not removed: error"), client->GetUserId());
            }
        } else {
            // not allowed
            serverSay(std::string("You are not authorized to unban people!"), client->GetUserId());
        }
    } else if (str.substr(0, 5) == "!ban ") {
        if (client->user.authstatus & RoRnet::AUTH_MOD || client->user.authstatus & RoRnet::AUTH_ADMIN) {
            int buid = -1;
            char banmsg_tmp[256] = "";
            int res = sscanf(str.substr(5).c_str(), "%d %s", &buid, banmsg_tmp);
            std::string banMsg = std::string(banmsg_tmp);
            banMsg = trim(banMsg);
            if (res != 2 || buid == -1 || !banMsg.size()) {
                serverSay(std::string("usage: !ban <client->GetUserId()> <message>"), client->GetUserId());
                serverSay(std::string("example: !ban 3 swearing"), client->GetUserId());
            } else {
                bool banned = Ban(buid, client->GetUserId(), str.substr(6 + intlen(buid), 256).c_str());
                if (!banned)
                    serverSay(std::string("kick + ban not successful: client->GetUserId() not found!"), client->GetUserId());
            }
        } else {
            // not allowed
            serverSay(std::string("You are not authorized to ban people!"), client->GetUserId());
        }
    } else if (str.substr(0, 6) == "!kick ") {
        if (client->user.authstatus & RoRnet::AUTH_MOD || client->user.authstatus & RoRnet::AUTH_ADMIN) {
            int kuid = -1;
            char kickmsg_tmp[256] = "";
            int res = sscanf(str.substr(6).c_str(), "%d %s", &kuid, kickmsg_tmp);
            std::string kickMsg = std::string(kickmsg_tmp);
            kickMsg = trim(kickMsg);
            if (res != 2 || kuid == -1 || !kickMsg.size()) {
                serverSay(std::string("usage: !kick <client->GetUserId()> <message>"), client->GetUserId());
                serverSay(std::string("example: !kick 3 bye!"), client->GetUserId());
            } else {
                bool kicked = Kick(kuid, client->GetUserId(), str.substr(7 + intlen(kuid), 256).c_str());
                if (!kicked)
                    serverSay(std::string("kick not successful: client->GetUserId() not found!"), client->GetUserId());
            }
        } else {
            // not allowed
            serverSay(std::string("You are not authorized to kick people!"), client->GetUserId());
        }
    } else if (str == "!vehiclelimit") {
        char sayMsg[128] = "";
        sprintf(sayMsg, "The vehicle-limit on this server is set on %d", Config::getMaxVehicles());
        serverSay(sayMsg, client->GetUserId(), FROM_SERVER);
    } else if (str.substr(0, 5) == "!say ") {
        if (client->user.authstatus & RoRnet::AUTH_MOD || client->user.authstatus & RoRnet::AUTH_ADMIN) {
            int kuid = -2;
            char saymsg_tmp[256] = "";
            int res = sscanf(str.substr(5).c_str(), "%d %s", &kuid, saymsg_tmp);
            std::string sayMsg = std::string(saymsg_tmp);

            sayMsg = trim(sayMsg);
            if (res != 2 || kuid < -1 || !sayMsg.size()) {
                serverSay(std::string("usage: !say <client->GetUserId()> <message> (use client->GetUserId() -1 for general broadcast)"), client->GetUserId());
                serverSay(std::string("example: !say 3 Wecome to this server!"), client->GetUserId());
            } else {
                serverSay(str.substr(6 + intlen(kuid), 256), kuid, FROM_HOST);
            }

        } else {
            // not allowed
            serverSay(std::string("You are not authorized to use this command!"), client->GetUserId());
        }
    } else if (str == "!website" || str == "!www") {
        if (!Config::getWebsite().empty()) {
            char sayMsg[256] = "";
            sprintf(sayMsg, "Further information can be found online at %s", Config::getWebsite().c_str());
            serverSay(sayMsg, client->GetUserId(), FROM_SERVER);
        }
    } else if (str == "!irc") {
        if (!Config::getIRC().empty()) {
            char sayMsg[256] = "";
            sprintf(sayMsg, "IRC: %s", Config::getIRC().c_str());
            serverSay(sayMsg, client->GetUserId(), FROM_SERVER);
        }
    } else if (str == "!owner") {
        if (!Config::getOwner().empty()) {
            char sayMsg[256] = "";
            sprintf(sayMsg, "This server is run by %s", Config::getOwner().c_str());
            serverSay(sayMsg, client->GetUserId(), FROM_SERVER);
        }
    } else if (str == "!voip") {
        if (!Config::getVoIP().empty()) {
            char sayMsg[256] = "";
            sprintf(sayMsg, "This server's official VoIP: %s", Config::getVoIP().c_str());
            serverSay(sayMsg, client->GetUserId(), FROM_SERVER);
        }
    } else if (str == "!rules") {
        if (!Config::getRulesFile().empty()) {
            std::vector<std::string> lines;
            int res = Utils::ReadLinesFromFile(Config::getRulesFile(), lines);
            if (!res) {
                std::vector<std::string>::iterator it;
                for (it = lines.begin(); it != lines.end(); it++) {
                    serverSay(*it, client->GetUserId(), FROM_RULES);
                }
            }
        }
    } else if (str == "!motd") {
        this->sendMOTD(client->GetUserId());
    }

    return publishMode;
}

broadcastType Sequencer::ProcessUtf8PrivChat(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
    // private chat message
    int destuid = *(int*)data;
    Client *dest_client = this->FindClientById(static_cast<unsigned int>(destuid));
    if (dest_client != nullptr) {
        char *chatmsg = data + sizeof(int);
        int chatlen = len - sizeof(int);
        dest_client->QueueMessage(RoRnet::MSG2_UTF8_PRIVCHAT, client->GetUserId(), streamid, chatlen, chatmsg);
    }
    return BROADCAST_BLOCK;
}

broadcastType Sequencer::ProcessGameCmd(Client* client, int type, unsigned int streamid, char *data, unsigned int len)
{
#ifdef WITH_ANGELSCRIPT
    if (m_script_engine) m_script_engine->gameCmd(client->GetUserId(), std::string(data));
#endif //WITH_ANGELSCRIPT
    return BROADCAST_BLOCK;
}
