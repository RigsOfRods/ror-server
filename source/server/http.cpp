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

#include "http.h"

#include "utils.h"
#include "logger.h"
#include "SocketW.h"

#include <assert.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace Http {

bool RequestRaw(
    const char* method,
    const char* host,
    const char* url,
    const char* content_type,
    const char* payload,
    char*       out_response_buffer,
    unsigned    response_buf_len)
{
    method = (method == nullptr) ? METHOD_GET : method;

    SWInetSocket socket;
    SWInetSocket::SWBaseError result;
    if (!socket.connect(80, host, &result) || (result != SWInetSocket::ok))
    {
        Logger::Log(LOG_ERROR,
            "Could not process HTTP %s request %s%s failed, error: ",
            method, host, url, result.get_error().c_str());
        return false;
    }
    char query[2000] = {0};
    char* query_pos = query;

    query_pos += sprintf(query_pos, "%s %s HTTP/1.1"     "\r\n", method, url);
    query_pos += sprintf(query_pos, "Host: %s"           "\r\n", host);
    query_pos += sprintf(query_pos, "Content-Type: %s"   "\r\n", content_type);
    query_pos += sprintf(query_pos, "Content-Length: %lu" "\r\n", static_cast<unsigned long>(strnlen(payload, 16000)));

    sprintf(query_pos, "\r\n%s", payload);

    if (socket.sendmsg(query, &result) < 0)
    {
        Logger::Log(LOG_ERROR,
            "Could not process HTTP %s request %s%s failed, error: ",
            method, host, url, result.get_error().c_str());
        return false;
    }

    int response_len = socket.recv(out_response_buffer, response_buf_len, &result);
    if (response_len < 0)
    {
        Logger::Log(LOG_ERROR,
            "Could not process HTTP %s request %s%s failed, invalid response length, error message: ",
            method, host, url, result.get_error().c_str());
        return false;
    }
    out_response_buffer[response_len] = 0;

    socket.disconnect();
    return true;
}

int Request(
    const char* method,
    const char* host,
    const char* url,
    const char* content_type,
    const char* payload,
    Response*   response)
{
    assert(response != nullptr);

    char response_buf[5000] = {0};
    if (!RequestRaw(method, host, url, content_type, payload, response_buf, 5000))
    {
        return -1;
    }

    std::string response_str(response_buf);
    if (!response->FromBuffer(response_str))
    {
        return -2;
    }

    return response->GetCode();
}

Response::Response():
    m_response_code(0)
{
}

const std::string& Response::GetBody()
{
    return m_headermap["body"];
}

const std::vector<std::string> Response::GetBodyLines()
{
    std::vector<std::string> lines;
    strict_tokenize( m_headermap["body"], lines, "\n" );
    return lines;
}

bool Response::IsChunked()
{
    return "chunked" == m_headermap["Transfer-Encoding"];
}

bool Response::FromBuffer(std::string& message)
{
    m_response_code = -1;
    m_headermap.clear();

    std::size_t locHolder;
    locHolder = message.find("\r\n\r\n");
    std::vector<std::string> header;
    std::vector<std::string> tmp;

    strict_tokenize( message.substr( 0, locHolder ), header, "\r\n" );

    m_response_code = atoi(header[0].c_str());
    for( unsigned short index = 1; index < header.size(); index++ )
    {
        tmp.clear();
        tokenize( header[index], tmp, ":" );
        if( tmp.size() != 2 )
        {
            continue;
        }
        m_headermap[ trim( tmp[0] ) ] = trim( tmp[1] );
    }

    tmp.clear();
    locHolder = message.find_first_not_of("\r\n", locHolder);
    if( std::string::npos == locHolder )
    {
        Logger::Log(LOG_ERROR, "Internal: HTTP message does not appear to contain a body: \n%s", message.c_str());
        return false;
    }

    strict_tokenize( message.substr( locHolder ), tmp, "\r\n" );
    if (IsChunked())
    {
        m_headermap["body"] = tmp[1];
    }
    else
    {
        m_headermap["body"] = tmp[0];
    }
}

} // namespace Http

