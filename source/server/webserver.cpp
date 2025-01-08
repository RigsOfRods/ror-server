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
along with "Rigs of Rods Server".
If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef WITH_WEBSERVER

#include "webserver.h"

#include "config.h"
#include "sequencer.h"
#include "userauth.h"

#include "Poco/JSON/Array.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include <iostream>

#include "web/index.h"
#include "web/playerlist.h"
#include "web/playerlist_test.h"

using namespace Poco;
using namespace Poco::Net;
using namespace Poco::JSON;

static bool s_is_advertised;
static int s_trust_level;
static Sequencer *s_sequencer;
static HTTPServer *srv;

class APIConfigHandler : public HTTPRequestHandler
{
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response)
    {
        response.setContentType("text/json");

        Object root;
        root.set("Max Clients", Config::getMaxClients());
        root.set("Server Name", Config::getServerName());
        root.set("Terrain Name", Config::getTerrainName());
        root.set("Password Protected", Config::getPublicPassword().empty() ? "No" : "Yes");
        root.set("IP Address", Config::getIPAddr() == "0.0.0.0" ? "0.0.0.0 (Any)" : Config::getIPAddr());
        root.set("Listening Port", Config::getListenPort());
        root.set("Protocol Version", std::string(RORNET_VERSION));

        std::stringstream s;
        Stringifier::stringify(root, s);
        response.send() << s.str();
    }
};

class APIPlayerListHandler : public HTTPRequestHandler
{
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response)
    {
        response.setContentType("text/json");

        Array root;

        std::vector<WebserverClientInfo> clients = s_sequencer->GetClientListCopy();
        for (auto it = clients.begin(); it != clients.end(); it++)
        {
            Object row;

            if (it->GetStatus() == Client::STATUS_FREE)
            {
                row.set("status", "FREE");
                root.add(row);
            }
            else if (it->GetStatus() == Client::STATUS_BUSY)
            {
                row.set("status", "BUSY");
                root.add(row);
            }
            else if (it->GetStatus() == Client::STATUS_USED)
            {
                // some auth identifiers
                std::string authst = "none";
                if (it->user.authstatus & RoRnet::AUTH_BANNED)
                    authst = "banned";
                if (it->user.authstatus & RoRnet::AUTH_BOT)
                    authst = "bot";
                if (it->user.authstatus & RoRnet::AUTH_RANKED)
                    authst = "ranked";
                if (it->user.authstatus & RoRnet::AUTH_MOD)
                    authst = "moderator";
                if (it->user.authstatus & RoRnet::AUTH_ADMIN)
                    authst = "admin";

                row.set("status", "USED");
                row.set("uid", it->user.uniqueid);
                row.set("ip", it->GetIpAddress());
                row.set("name", it->user.username);
                row.set("auth", authst);

                root.add(row);
            }
        }

        std::stringstream s;
        Stringifier::stringify(root, s, 1);
        response.send() << s.str();
    }
};

class NotFoundHandler : public HTTPRequestHandler
{
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response)
    {
        response.setContentType("text/html");

        response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);

        response.send() << "Not found: " << request.getURI();
    }
};

class RequestHandlerFactory : public HTTPRequestHandlerFactory
{
    HTTPRequestHandler *createRequestHandler(const HTTPServerRequest &request)
    {
        if (request.getURI() == "/")
            return new IndexHandler();
        else if (request.getURI() == "/playerlist")
            return new PlayerlistHandler();
        else if (request.getURI() == "/playerlist-test")
            return new Playerlist_testHandler(s_sequencer->GetClientListCopy());
        else if (request.getURI() == "/api/playerlist")
            return new APIPlayerListHandler();
        else if (request.getURI() == "/api/config")
            return new APIConfigHandler();
        else
            return new NotFoundHandler();
    }
};

int StartWebserver(int port, Sequencer *sequencer, bool is_advertised, int trust_level)
{
    s_is_advertised = is_advertised;
    s_trust_level = trust_level;
    s_sequencer = sequencer;

    srv = new HTTPServer(new RequestHandlerFactory, port);
    srv->start();

    return 0;
}

int StopWebserver()
{
    srv->stop();
    return 0;
}
#endif