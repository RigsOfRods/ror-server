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

#pragma once

#include "prerequisites.h"
#include "UnicodeStrings.h"

#include "json/json.h"

namespace MasterServer {

class Client
{
public:
    Client();

    bool Register();
    bool SendHeatbeat(Json::Value user_list);
    bool UnRegister();
    bool IsRegistered() const { return m_is_registered; }
    int  GetTrustLevel() const { return m_trust_level; }

private:
    int HttpRequest(const char* method, const char* payload, Http::Response* out_response); ///< Helper

    std::string   m_token;
    int           m_trust_level;
    bool          m_is_registered;
    std::string   m_server_path;
};

bool RetrievePublicIp();

} // namespace MasterServer

