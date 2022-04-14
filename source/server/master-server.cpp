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
#include "utils.h"

#include <assert.h>

#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/ParseHandler.h>
#include <Poco/JSON/JSONException.h>
#include <Poco/Dynamic/Var.h>

using namespace Poco;
using namespace Poco::JSON;
using namespace Poco::Dynamic;

namespace MasterServer {

    Client::Client() :
            m_trust_level(-1),
            m_is_registered(false) {}

    bool Client::Register() {
        DynamicStruct data;
        data["ip"] = Config::getIPAddr();
        data["port"] = Config::getListenPort();
        data["name"] = Config::getServerName();
        data["terrain-name"] = Config::getTerrainName();
        data["max-clients"] = Config::getMaxClients();
        data["version"] = RORNET_VERSION;
        data["use-password"] = Config::isPublic();

        m_server_path = "/" + Config::GetServerlistPath() + "/server-list";

        auto json_str = JsonToString(data);

        Logger::Log(LOG_INFO, "Attempting to register on serverlist (%s)", m_server_path.c_str());
        Http::Response response;
        int result_code = this->HttpRequest(Http::METHOD_POST, json_str.c_str(), &response);
        if (result_code < 0) {
            Logger::Log(LOG_ERROR, "Registration failed, result code: %d", result_code);
            return false;
        } else if (result_code != 200) {
            Logger::Log(LOG_INFO, "Registration failed, response code: HTTP %d, body: %s", result_code,
                        response.GetBody().c_str());
            return false;
        }

        Parser parser;
        Var result;

        try
        {
            result = parser.parse(response.GetBody());
        }
        catch(JSONException& jsonException)
        {
            Logger::Log(LOG_ERROR, "Registration failed, invalid server response (%s)", jsonException.message().c_str());
            Logger::Log(LOG_DEBUG, "Raw response: %s", response.GetBody().c_str());
            return false;
        }

        DynamicStruct root = result.extract<Object>();

        m_trust_level = root["verified-level"];
        m_token = root["challenge"].toString();
        m_is_registered = true;
        return true;
    }

    bool Client::SendHeatbeat(Poco::JSON::Array &user_list) {
        DynamicStruct data;
        data["challenge"] = m_token;
        data["users"] = user_list;

        auto json_str = JsonToString(data);
        Logger::Log(LOG_DEBUG, "Heartbeat JSON:\n%s", json_str.c_str());

        Http::Response response;
        int result_code = this->HttpRequest(Http::METHOD_PUT, json_str.c_str(), &response);
        if (result_code != 200) {
            const char *type = (result_code < 0) ? "result code" : "HTTP code";
            Logger::Log(LOG_ERROR, "Heatbeat failed, %s: %d", type, result_code);
            return false;
        }
        return true;
    }

    bool Client::UnRegister() {
        assert(m_is_registered == true);

        DynamicStruct data;
        data["challenge"] = m_token;

        auto json_str = JsonToString(data);

        Logger::Log(LOG_DEBUG, "UnRegister JSON:\n%s", json_str.c_str());

        Http::Response response;
        int result_code = this->HttpRequest(Http::METHOD_DELETE, json_str.c_str(), &response);
        if (result_code < 0) {
            const char *type = (result_code < 0) ? "result code" : "HTTP code";
            Logger::Log(LOG_ERROR, "Failed to un-register server, %s: %d", type, result_code);
            return false;
        }
        m_is_registered = false;
        return true;
    }

// Helper
    int Client::HttpRequest(const char *method, const char *payload, Http::Response *out_response) {
        return Http::Request(method, Config::GetServerlistHostC(), m_server_path.c_str(), "application/json", payload,
                             out_response);
    }

    bool RetrievePublicIp() {
        char url[300] = "";
        sprintf(url, "/%s/get-public-ip", Config::GetServerlistPath().c_str());

        Http::Response response;
        int result_code = Http::Request(Http::METHOD_GET,
                                        Config::GetServerlistHostC(), url, "application/json", "", &response);
        if (result_code < 0) {
            Logger::Log(LOG_ERROR, "Failed to retrieve public IP address");
            return false;
        }
        Config::setIPAddr(response.GetBody());
        return true;
    }

} // namespace MasterServer

