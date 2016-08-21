/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2016   Petr Ohlídal
Copyright 2016+  Rigs of Rods Community

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

#include "master-server.h"

#include "config.h"
#include "rornet.h"
#include "logger.h"
#include "http.h"
#include "json/json.h"

#include <assert.h>

namespace MasterServer {

Client::Client():
    m_trust_level(-1),
    m_is_registered(false)
{}

bool Client::Register()
{
    Json::Value data(Json::objectValue);
    data["ip"]           = Config::getIPAddr();
    data["port"]         = Config::getListenPort();
    data["name"]         = Config::getServerName();
    data["terrain-name"] = Config::getTerrainName();
    data["max-clients"]  = Config::getMaxClients();
    data["version"]      = RORNET_VERSION;
    data["use-password"] = Config::isPublic();

    char url[200];
    sprintf(url, "/%s/server-list", Config::GetServerlistPathC());

    Logger::Log(LOG_INFO, "Attempting to register on serverlist (%s)", url);
    Http::Response response;
    int result_code = Http::Request(Http::METHOD_POST, Config::GetServerlistHostC(), url,
        "application/json", data.toStyledString().c_str(), &response);
    if (result_code < 0)
    {
        Logger::Log(LOG_ERROR, "Registration failed, response code: HTTP %d", response.GetCode());
        return false;
    }
    else if (result_code != 200)
    {
        Logger::Log(LOG_INFO, "Registration failed, response code: HTTP %d, body: %s", response.GetCode(), response.GetBody().c_str());
        return false;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(response.GetBody().c_str(), root))
    {
        Logger::Log(LOG_ERROR, "Registration failed, couldn't parse response");
        return false;
    }

    Json::Value trust_level = root["verified-level"];
    Json::Value challenge = root["challenge"];
    if (!root.isObject() || !trust_level.isNumeric() || !challenge.isString())
    {
        Logger::Log(LOG_ERROR, "Registration failed, incorrect response from server");
        return false;
    }

    m_token = challenge.asString();
    m_trust_level = trust_level.asInt();
    m_is_registered = true;
    return true;
}

bool Client::SendHeatbeat(Json::Value user_list)
{
    const char* host = Config::GetServerlistHost().c_str();

    char url[200] = { 0 };
    sprintf(url, "%s/server-list/", host);

    Json::Value data(Json::objectValue);
    data["challenge"] = m_token;
    data["users"] = user_list;

    Http::Response response;
    int result_code = Http::Request(Http::METHOD_PUT, host, url, "application/json", data.asCString(), &response);
    if (result_code < 0)
    {
        Logger::Log(LOG_ERROR, "Heatbeat failed");
        return false;
    }
    return true;
}

bool Client::UnRegister()
{
    assert(m_is_registered == true);

    const char* host = Config::GetServerlistHost().c_str();

    char url[200] = { 0 };
    sprintf(url, "%s/server-list/?challenge=%s", host, m_token.c_str());

    Http::Response response;
    int result_code = Http::Request(Http::METHOD_DELETE, host, url, "application/json", "", &response);
    if (result_code < 0)
    {
        Logger::Log(LOG_ERROR, "Failed to unregister server");
        return false;
    }
    m_is_registered = false;
    return true;
}

bool RetrievePublicIp()
{
    char url[300] = "";
    sprintf(url, "/%s/get-public-ip", Config::GetServerlistPathC());

    Http::Response response;
    int result_code = Http::Request(Http::METHOD_GET,
        Config::GetServerlistHostC(), url, "application/json", "", &response);
    if (result_code < 0)
    {
        Logger::Log(LOG_ERROR, "Failed to retrieve public IP address");
        return false;
    }
    Config::setIPAddr(response.GetBody());
    return true;
}

} // namespace MasterServer

