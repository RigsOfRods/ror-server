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

bool Listener::Initialize() {
    // Make sure it's not started twice
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread_state != ThreadState::NOT_RUNNING)
    {
        return true;
    }

    // Start listening on the socket
    SWBaseSocket::SWBaseError error;
    m_listen_socket.bind(Config::getListenPort(), &error);
    if (error != SWBaseSocket::ok) {
        Logger::Log(LOG_ERROR, "FATAL Listerer: %s", error.get_error().c_str());
        return false;
    }
    m_listen_socket.listen();

    // Start the thread
    m_thread = std::thread(&Listener::ThreadMain, this);
    m_thread_state = ThreadState::RUNNING;

    return true;
}

void Listener::Shutdown() {
    // Make sure it's not shut down twice
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread_state != ThreadState::RUNNING)
    {
        return;
    }

    Logger::Log(LOG_VERBOSE, "Stopping listener thread...");
    m_thread_state = ThreadState::STOP_REQUESTED;
    m_thread.join();
    Logger::Log(LOG_VERBOSE, "Listener thread stopped");
}

void Listener::ThreadMain() {
    Logger::Log(LOG_DEBUG, "Listerer thread starting");

    SWBaseSocket::SWBaseError error;

    //await connections
    while (GetThreadState() == ThreadState::RUNNING) {
        Logger::Log(LOG_VERBOSE, "Listener awaiting connections");
        SWInetSocket *ts = (SWInetSocket *) m_listen_socket.accept(&error);
        if (error != SWBaseSocket::ok) {
            if (GetThreadState() == ThreadState::STOP_REQUESTED) {
                Logger::Log(LOG_ERROR, "INFO Listener shutting down");
            } else {
                Logger::Log(LOG_ERROR, "ERROR Listener: %s", error.get_error().c_str());
            }
        }

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
            if (Messaging::SWReceiveMessage(ts, &type, &source, &streamid, &len,
                                            buffer, RORNET_MAX_MESSAGE_LENGTH))
                throw std::runtime_error("ERROR Listener: receiving first message");

            // make sure our first message is a hello message
            if (type != RoRnet::MSG2_HELLO) {
                Messaging::SWSendMessage(ts, RoRnet::MSG2_WRONG_VER, 0, 0, 0, 0);
                throw std::runtime_error("ERROR Listener: protocol error");
            }

            // check client version
            if (source == 5000 && (std::string(buffer) == "MasterServer")) {
                Logger::Log(LOG_VERBOSE, "Master Server knocked ...");
                // send back some information, then close socket
                char tmp[2048] = "";
                sprintf(tmp, "protocol:%s\nrev:%s\nbuild_on:%s_%s\n", RORNET_VERSION, VERSION, __DATE__, __TIME__);
                if (Messaging::SWSendMessage(ts, RoRnet::MSG2_MASTERINFO, 0, 0, (unsigned int) strlen(tmp), tmp)) {
                    throw std::runtime_error("ERROR Listener: sending master info");
                }
                // close socket
                ts->disconnect(&error);
                delete ts;
                continue;
            }

            // compare the versions if they are compatible
            if (strncmp(buffer, RORNET_VERSION, strlen(RORNET_VERSION))) {
                // not compatible
                Messaging::SWSendMessage(ts, RoRnet::MSG2_WRONG_VER, 0, 0, 0, 0);
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

            if (Messaging::SWSendMessage(ts, RoRnet::MSG2_HELLO, 0, 0, (unsigned int) sizeof(RoRnet::ServerInfo),
                                       (char *) &settings))
                throw std::runtime_error("ERROR Listener: sending version");

            //receive user infos
            if (Messaging::SWReceiveMessage(ts, &type, &source, &streamid, &len,
                                            buffer,
                                            RORNET_MAX_MESSAGE_LENGTH)) {
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
            user->authstatus = m_sequencer->AuthorizeNick(std::string(user->usertoken, 40), std::string(user->authtoken, 300), nickname);
            strncpy(user->username, nickname.c_str(), RORNET_MAX_USERNAME_LEN - 1);

            if (Config::isPublic()) {
                Logger::Log(LOG_DEBUG, "password login: %s == %s?",
                            Config::getPublicPassword().c_str(),
                            std::string(user->serverpassword, 40).c_str());
                if (strncmp(Config::getPublicPassword().c_str(), user->serverpassword, 40)) {
                    Messaging::SWSendMessage(ts, RoRnet::MSG2_WRONG_PW, 0, 0, 0, 0);
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
            ts->disconnect(&error);
            delete ts;
        }
    }
}

Listener::ThreadState Listener::GetThreadState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_thread_state;
}
