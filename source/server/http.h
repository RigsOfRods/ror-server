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

#pragma once

#include "prerequisites.h"
#include "UnicodeStrings.h"

#include <map>
#include <vector>

namespace Http {

    extern const char *METHOD_GET;
    extern const char *METHOD_POST;
    extern const char *METHOD_PUT;
    extern const char *METHOD_DELETE;

    class Response {
    public:
        Response();

        const std::string &GetBody();

        const std::vector<std::string> GetBodyLines();

        bool IsChunked();

        bool FromBuffer(const std::string &message);

        int GetCode() { return m_response_code; }

    private:
        std::map<std::string, std::string> m_headermap;
        int m_response_code;
    };

    std::string RequestRaw(
            std::string method,
            std::string host,
            std::string url,
            std::string content_type,
            std::string payload);

    int Request(
            std::string method,
            std::string host,
            std::string url,
            std::string content_type,
            std::string payload,
            Response *out_response);

    void Register(asIScriptEngine* engine);

} // namespace Http

