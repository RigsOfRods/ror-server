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

#include "config.h"
#include "messaging.h"
#include "utils.h"

Listener::Listener(Sequencer *sequencer, kissnet::port_t listen_port)
    : m_sequencer(sequencer)
    , m_socket(kissnet::endpoint("0.0.0.0", listen_port))
{
    m_socket.bind();
    m_socket.listen();
    m_thread = std::thread(&Listener::ListenThread, this);
}

Listener::~Listener()
{
    try
    {
        this->Shutdown();
    }
    catch (std::exception& e)
    {
        Logger::Log(LOG_ERROR, "Listener destructor: %s", e.what());
    }
}

void Listener::ListenThread()
{
    for(;;) // Endless loop
    {
        try
        {
            // Block until somebody connects or socket is closed by main thread.
            kissnet::tcp_socket conn_sock = m_socket.accept();
            if (!m_socket.is_valid())
            {
                Logger::Log(LOG_VERBOSE, "ListenThread(): Invalid socket after `accept()`, exiting.");
                m_socket.close();
                return;
            }

            // Evaluate the connection
            Logger::Log(LOG_VERBOSE, "ListenThread(): got a new connection");
            RoRnet::UserInfo user;
            if (this->CheckClient(conn_sock, &user))
            {
                m_sequencer->createClient(std::move(conn_sock), user);
            }
            else
            {
                conn_sock.close();
            }
        }
        catch (std::exception& e)
        {
            Logger::Log(LOG_ERROR, "ListenThread(): %s", e.what());
            if (!m_socket.is_valid())
            {
                Logger::Log(LOG_VERBOSE, "ListenThread(): Invalid socket after exception, exiting.");
                m_socket.close();
                return;
            }
        }
    }
}

bool Listener::CheckClient(kissnet::tcp_socket& socket, RoRnet::UserInfo *user)
{
    RoRnet::Header header;
    RoRnet::Payload payload;

    // this is the start of it all, it all starts with a simple hello
    if (Messaging::ReceiveMessage(socket, header, payload))
        throw std::runtime_error("ERROR Listener: receiving first message");

    // make sure our first message is a hello message
    if (header.command != RoRnet::MSG2_HELLO) {
        Messaging::SendMessage(socket, RoRnet::MSG2_WRONG_VER, 0, 0, 0, 0);
        Logger::Log(LOG_INFO, "ListenThread(): Client sent invalid request, rejecting.");
        return false; // Close connection
    }

    std::string version_str(reinterpret_cast<char*>(payload.data()));

    // Magic 'source_id' 5000 is used by serverlist to check this server.
    if (header.source == 5000 && version_str == "MasterServer") {
        Logger::Log(LOG_VERBOSE, "Master Server knocked ...");
        // send back some information, then close socket
        char tmp[2048] = "";
        sprintf(tmp, "protocol:%s\nrev:%s\nbuild_on:%s_%s\n", RORNET_VERSION, VERSION, __DATE__, __TIME__);
        if (Messaging::SendMessage(socket, RoRnet::MSG2_MASTERINFO, 0, 0, (unsigned int) strlen(tmp), tmp)) {
            throw std::runtime_error("ListenThread(): error sending master info");
        }
        return false; // Caller will close the socket
    }

    // compare the versions if they are compatible
    if (version_str != RORNET_VERSION) {
        // not compatible
        Messaging::SendMessage(socket, RoRnet::MSG2_WRONG_VER, 0, 0, 0, 0);
        Logger::Log(LOG_INFO, "ListenThread(): Rejecting client, bad RoRnet version (%s)", version_str.c_str());
        return false; // Close connection
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

    if (Messaging::SendMessage(socket, RoRnet::MSG2_HELLO, 0, 0, (unsigned int) sizeof(RoRnet::ServerInfo),
                                (char *) &settings))
    {
        throw std::runtime_error("ListenThread(): failed to send version");
    }

    //receive user infos
    payload.fill(std::byte(0));
    if (Messaging::ReceiveMessage(socket, header, payload))
    {
        throw std::runtime_error("ListenThread(): failed to receive user info");
    }

    if (header.command != RoRnet::MSG2_USER_INFO)
        throw std::runtime_error("Warning Listener: no user name");

    if (header.size > sizeof(RoRnet::UserInfo))
        throw std::runtime_error("Error: did not receive proper user credentials");
    Logger::Log(LOG_INFO, "Listener creating a new client...");

    std::memcpy(user, payload.data(), sizeof(RoRnet::UserInfo));
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
            Messaging::SendMessage(socket, RoRnet::MSG2_WRONG_PW, 0, 0, 0, 0);
            throw std::runtime_error("ERROR Listener: wrong password");
        }

        Logger::Log(LOG_DEBUG, "user used the correct password, "
                "creating client!");
    } else {
        Logger::Log(LOG_DEBUG, "no password protection, creating client");
    }

    return true;
}

void Listener::Shutdown()
{
    if (m_thread.joinable())
    {
        m_socket.close();
        m_thread.join();
    }
}
