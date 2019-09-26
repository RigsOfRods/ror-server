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
#include "prerequisites.h"

#include <assert.h>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <iostream>
#include <cstring>

namespace Http {

    const char *METHOD_GET = "GET";
    const char *METHOD_POST = "POST";
    const char *METHOD_PUT = "PUT";
    const char *METHOD_DELETE = "DELETE";


    std::string RequestRaw(
            std::string method,
            std::string host,
            std::string url,
            std::string content_type,
            std::string payload) {
        method = method.empty() ? METHOD_GET : method;

        kissnet::tcp_socket socket(kissnet::endpoint(host, 80));
        try
        {
            socket.connect();
            std::string query = method + " " + url + " HTTP/1.1\r\nHost: " + host + "\r\nContent-Type: " +
                content_type + "\r\nContent-Length: " + std::to_string(payload.length()) + "\r\n\r\n" + payload;
            auto [sent_len, sock_sent_status] = socket.send((std::byte*)query.data(), query.length());
            if (sent_len != query.length() || sock_sent_status != kissnet::socket_status::valid)
            {
                Logger::Log(LOG_ERROR,
                    "Could not process HTTP %s request %s%s failed, error sending data",
                    method, host, url);
                socket.close();
                return "";
            }

            kissnet::buffer<5000> recv_buffer;
            auto [recv_len, sock_recv_status] = socket.recv(recv_buffer);
            if (recv_len == 0 || sock_recv_status != kissnet::socket_status::valid)
            {
                Logger::Log(LOG_ERROR,
                    "Could not process HTTP %s request %s%s failed, error receiving data",
                    method, host, url);
                socket.close();
                return "";
            }

            recv_buffer[std::min(recv_buffer.size() - 1, recv_len)] = std::byte(0); // Ensure null-terminated string
            return std::string((const char*)recv_buffer.data());
        }
        catch (std::exception& e)
        {
            Logger::Log(LOG_ERROR,
                "Could not process HTTP %s request %s%s failed, message: %s",
                method, host, url, e.what());
        }
        return "";
    }

    int Request(
            std::string method,
            std::string host,
            std::string url,
            std::string content_type,
            std::string payload,
            Response *response) {
        assert(response != nullptr);

        std::string response_str = RequestRaw(method, host, url, content_type, payload);

        if (response_str.empty()) {
            return -1;
        }

        if (!response->FromBuffer(response_str)) {
            return -2;
        }

        return response->GetCode();
    }

    Response::Response() :
            m_response_code(0) {
    }

    const std::string &Response::GetBody() {
        return m_headermap["body"];
    }

    const std::vector<std::string> Response::GetBodyLines() {
        std::vector<std::string> lines;
        strict_tokenize(m_headermap["body"], lines, "\n");
        return lines;
    }

    bool Response::IsChunked() {
        return "chunked" == m_headermap["Transfer-Encoding"];
    }

    bool Response::FromBuffer(const std::string &message) {
        m_response_code = -1;
        m_headermap.clear();

        std::size_t locHolder;
        locHolder = message.find("\r\n\r\n");
        std::vector<std::string> header;
        std::vector<std::string> tmp;

        strict_tokenize(message.substr(0, locHolder), header, "\r\n");

        // Process response code. The input has form "HTTP/1.1 200 OK"
        char line[200];
        strcpy(line, header[0].c_str());
        char *tok0 = std::strtok(line, " ");
        char *tok1 = std::strtok(nullptr, " ");
        if (tok0 == nullptr || tok1 == nullptr) {
            Logger::Log(LOG_ERROR, "Internal: HTTP response has malformed 1st line: \n%s", header[0].c_str());
            return false;
        }
        m_response_code = atoi(tok1);

        // Process header lines
        for (unsigned short index = 1; index < header.size(); index++) {
            tmp.clear();
            tokenize(header[index], tmp, ":");
            if (tmp.size() != 2) {
                continue;
            }
            m_headermap[trim(tmp[0])] = trim(tmp[1]);
        }

        tmp.clear();
        locHolder = message.find_first_not_of("\r\n", locHolder);
        if (std::string::npos == locHolder) {
            Logger::Log(LOG_ERROR, "Internal: HTTP message does not appear to contain a body: \n%s", message.c_str());
            return false;
        }

        strict_tokenize(message.substr(locHolder), tmp, "\r\n");
        if (IsChunked()) {
            m_headermap["body"] = tmp[1];
        } else {
            m_headermap["body"] = tmp[0];
        }
        return true;
    }

} // namespace Http

