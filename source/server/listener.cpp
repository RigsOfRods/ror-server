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

#include "listener.h"

#include "rornet.h"
#include "messaging.h"
#include "sequencer.h"
#include "SocketW.h"
#include "logger.h"
#include "config.h"
#include "UnicodeStrings.h"
#include "utils.h"

#include <stdexcept>
#include <sstream>
#include <stdio.h>

#ifdef __GNUC__

#include <stdlib.h>

#endif

Listener::Listener(Sequencer *sequencer) :
    m_sequencer(sequencer) {
}


Listener::~Listener(void) {
    std::lock_guard<std::mutex> scoped_lock(m_mutex);
    assert(m_state == NOT_RUNNING);
}


bool Listener::Start() {
    std::lock_guard<std::mutex> scoped_lock(m_mutex);
    assert(m_state == NOT_RUNNING);

    // Synchronously bind the socket and start listening
    SWBaseSocket::SWBaseError result;
    m_listen_socket.bind((int)Config::getListenPort(), &result);
    if (result != SWBaseSocket::ok) {
        Logger::Log(LOG_ERROR, "Listener: FATAL, cannot listen for connections - bind() failed: %s", result.get_error().c_str());
        return false;
    }
    m_listen_socket.listen((int)Config::getListenQueueLen(), &result);
    if (result != SWBaseSocket::ok) {
        Logger::Log(LOG_ERROR, "Listener: FATAL, cannot listen for connections - listen() failed: %s", result.get_error().c_str());
        return false;
    }

    // Start thread
    m_thread = std::thread(&Listener::Thread, this);
    m_state = RUNNING;
}


void Listener::Stop() {
    {
        std::lock_guard<std::mutex> scoped_lock(m_mutex);
        if (m_state == RUNNING)
            m_state = STOP_REQUESTED;
        else if (m_state != THREAD_STOPPED)
            return;
    }

    m_listen_socket.close_fd(); // Close file descriptor - this is thread safe.
    m_thread.join(); // Wait for thread to exit (will do nothing if not running anymore)

    {
        std::lock_guard<std::mutex> scoped_lock(m_mutex);
        m_state = NOT_RUNNING;
    }
}


void Listener::Thread() {
    Logger::Log(LOG_VERBOSE, "Listener awaiting connections");
    while (true) {
        // Wait for connection (or socket close)
        SWBaseSocket::SWBaseError result;
        SWBaseSocket* new_socket = m_listen_socket.accept(&result);

        // Evaluate result.
        if (result != SWBaseSocket::ok) {
            std::lock_guard<std::mutex> scoped_lock(m_mutex);
            if (m_state == STOP_REQUESTED) {
                break; // The error is just closed socket, ignore it and exit thread.
            } else {
                Logger::Log(LOG_ERROR, "Listener - Error while accepting new connection - accept() failed: %s", result.get_error().c_str());
                break;
            }
        } else {
            std::lock_guard<std::mutex> scoped_lock(m_mutex);
            if (m_state == STOP_REQUESTED) {
                new_socket->disconnect(&result);
                if (result != SWBaseSocket::ok) {
                    Logger::Log(LOG_ERROR, "Listener - Could not disconnect new client after thread shutdown request - disconnect() failed: %s", result.get_error().c_str());
                }
                break;
            }
        }

        // Process new connection.
        this->ThreadHandleConnection((SWInetSocket*)new_socket);
    }

    {
        std::lock_guard<std::mutex> scoped_lock(m_mutex);
        m_state = THREAD_STOPPED;
    }
}


void Listener::ThreadHandleConnection(SWInetSocket* ts) {

    Logger::Log(LOG_VERBOSE, "Listener got a new connection");

    ts->set_timeout(5, 0);

    //receive a magic
    int type;
    int source;
    unsigned int len;
    unsigned int streamid;
    char buffer[RORNET_MAX_MESSAGE_LENGTH];

    try {
        // this is the start of it all, it all starts with a simple hello
        if (Messaging::ReceiveMessage(ts, &type, &source, &streamid, &len, buffer, RORNET_MAX_MESSAGE_LENGTH))
            throw std::runtime_error("ERROR Listener: receiving first message");

        // make sure our first message is a hello message
        if (type != RoRnet::MSG2_HELLO) {
            Messaging::SendMessage(ts, RoRnet::MSG2_WRONG_VER, 0, 0, 0, 0);
            throw std::runtime_error("ERROR Listener: protocol result");
        }

        // check client version
        if (source == 5000 && (std::string(buffer) == "MasterServer")) {
            Logger::Log(LOG_VERBOSE, "Master Server knocked ...");
            // send back some information, then close socket
            char tmp[2048] = "";
            sprintf(tmp, "protocol:%s\nrev:%s\nbuild_on:%s_%s\n", RORNET_VERSION, VERSION, __DATE__, __TIME__);
            if (Messaging::SendMessage(ts, RoRnet::MSG2_MASTERINFO, 0, 0, (unsigned int) strlen(tmp), tmp)) {
                throw std::runtime_error("ERROR Listener: sending master info");
            }
            // close socket
            SWBaseSocket::SWBaseError result;
            ts->disconnect(&result);
            if (result != SWBaseSocket::ok) {
                Logger::Log(LOG_ERROR, "Listener - Could not disconnect master server after version check - disconnect() failed: %s", result.get_error().c_str());
            }
            delete ts;
            return;
        }

        // compare the versions if they are compatible
        if (strncmp(buffer, RORNET_VERSION, strlen(RORNET_VERSION))) {
            // not compatible
            Messaging::SendMessage(ts, RoRnet::MSG2_WRONG_VER, 0, 0, 0, 0);
            throw std::runtime_error("ERROR Listener: bad version: " + std::string(buffer) + ". rejecting ...");
        }

        // compatible version, continue to send server settings
        std::string motd_str;
        {
            std::vector<std::string> lines;
            if (!Utils::ReadLinesFromFile(Config::getMOTDFile(), lines))
            {
                for (const auto& line : lines)
                    motd_str += line + "\n";
            }
        }

        Logger::Log(LOG_DEBUG, "Listener sending server settings");
        RoRnet::ServerInfo settings;
        memset(&settings, 0, sizeof(RoRnet::ServerInfo));
        settings.has_password = !Config::getPublicPassword().empty();
        strncpy(settings.info, motd_str.c_str(), motd_str.size());
        strncpy(settings.protocolversion, RORNET_VERSION, strlen(RORNET_VERSION));
        strncpy(settings.servername, Config::getServerName().c_str(), Config::getServerName().size());
        strncpy(settings.terrain, Config::getTerrainName().c_str(), Config::getTerrainName().size());

        if (Messaging::SendMessage(ts, RoRnet::MSG2_HELLO, 0, 0, (unsigned int) sizeof(RoRnet::ServerInfo),
                                    (char *) &settings))
            throw std::runtime_error("ERROR Listener: sending version");

        //receive user infos
        if (Messaging::ReceiveMessage(ts, &type, &source, &streamid, &len, buffer, RORNET_MAX_MESSAGE_LENGTH)) {
            std::stringstream error_msg;
            error_msg << "ERROR Listener: receiving user infos\n"
                        << "ERROR Listener: got that: "
                        << type;
            throw std::runtime_error(error_msg.str());
        }

        if (type != RoRnet::MSG2_USER_INFO)
            throw std::runtime_error("Warning Listener: no user name");

        if (len > sizeof(RoRnet::UserInfo))
            throw std::runtime_error("Error: did not receive proper user credentials");
        Logger::Log(LOG_INFO, "Listener creating a new client...");

        RoRnet::UserInfo *user = (RoRnet::UserInfo *) buffer;
        user->authstatus = RoRnet::AUTH_NONE;

        // authenticate
        user->username[RORNET_MAX_USERNAME_LEN - 1] = 0;
        std::string nickname = Str::SanitizeUtf8(user->username);
        user->authstatus = m_sequencer->AuthorizeNick(std::string(user->usertoken, 40), nickname);
        strncpy(user->username, nickname.c_str(), RORNET_MAX_USERNAME_LEN - 1);

        if (Config::isPublic()) {
            Logger::Log(LOG_DEBUG, "password login: %s == %s?",
                        Config::getPublicPassword().c_str(),
                        std::string(user->serverpassword, 40).c_str());
            if (strncmp(Config::getPublicPassword().c_str(), user->serverpassword, 40)) {
                Messaging::SendMessage(ts, RoRnet::MSG2_WRONG_PW, 0, 0, 0, 0);
                throw std::runtime_error("ERROR Listener: wrong password");
            }

            Logger::Log(LOG_DEBUG, "user used the correct password, "
                    "creating client!");
        } else {
            Logger::Log(LOG_DEBUG, "no password protection, creating client");
        }

        //create a new client
        m_sequencer->createClient(ts, *user); // copy the user info, since the buffer will be cleared soon
        Logger::Log(LOG_DEBUG, "listener returned!");
    }
    catch (std::runtime_error &e) {
        Logger::Log(LOG_ERROR, e.what());
        SWBaseSocket::SWBaseError result;
        ts->disconnect(&result);
        if (result != SWBaseSocket::ok) {
            Logger::Log(LOG_ERROR, "Listener - Could not disconnect client after rejection - disconnect() failed: %s", result.get_error().c_str());
        }
        delete ts;
    }
}
