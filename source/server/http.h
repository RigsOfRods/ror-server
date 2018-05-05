/*
 * This file is part of "Rigs of Rods Server" (Relay mode)
 *
 * Copyright 2007   Pierre-Michel Ricordel
 * Copyright 2014+  Rigs of Rods Community
 *
 * "Rigs of Rods Server" is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * "Rigs of Rods Server" is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with "Rigs of Rods Server". If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "UnicodeStrings.h"
#include <map>
#include <vector>

namespace Http
{
    extern const char *METHOD_GET;
    extern const char *METHOD_POST;
    extern const char *METHOD_PUT;
    extern const char *METHOD_DELETE;

    class Response
    {
public:
        Response();

        const std::string &GetBody();

        const std::vector<std::string> GetBodyLines();

        bool IsChunked();

        bool FromBuffer(const std::string &message);

        int GetCode()
        {
            return m_response_code;
        }

private:
        std::map<std::string, std::string> m_headermap;
        int                                m_response_code;
    };

    bool RequestRaw(
        const char *method,
        const char *host,
        const char *url,
        const char *content_type,
        const char *payload,
        char *out_response_buffer,
        unsigned response_buf_len);

    int Request(
        const char *method,
        const char *host,
        const char *url,
        const char *content_type,
        const char *payload,
        Response *out_response);
} // namespace Http

