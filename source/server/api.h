/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2025 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/
///@file api.h

#include <string>
#include <vector>

#include <rornet.h>

/**
 * \brief Enum representing different API error states
 */
enum ApiErrorState
{
    API_NO_ERROR = 0,
    API_CLIENT_ERROR = 1,
    API_SERVER_ERROR = 2,
    API_UNKNOWN_ERROR = 999,
};

namespace Api
{
    class Client
    {
        /**
         * \brief Enum representing different HTTP methods
         */
        enum HttpMethod
        {
            GET,
            POST,
            PUT,
            DELETE,
            PATCH,
            UPDATE
        };

        /**
         * \brief Sructure representing an HTTP response
         */
        struct HttpResponse
        {
            int status_code;
            std::string body;
            std::string headers;
        };

        /**
         * \brief Structure representing an HTTP request
         */
        struct HttpRequest
        {
            HttpMethod method = HttpMethod::GET;
            std::string url;
            std::string body;
            std::vector<std::string> headers;
            std::string content_type;
            std::string user_agent;

            HttpRequest(HttpMethod method,
                        const std::string &uri,
                        const std::string &body = "",
                        const std::vector<std::string> &headers = {},
                        const std::string &content_type = "Content-Type: application/json",
                        const std::string &user_agent = std::string("Rigs of Rods Server/") + RORNET_VERSION)
                : method(method),
                  url("http://127.0.0.1:8080" + uri),
                  body(body),
                  headers(headers),
                  content_type(content_type),
                  user_agent(user_agent)
            {
            }
        };

    public:
        Client();
        bool GetPublicIp(std::string &ip_addr);
        bool Callable();
        bool Authenticated();
        ApiErrorState CreateServer();
        ApiErrorState UpdateServer();
        ApiErrorState SyncServer();
        ApiErrorState SyncServerPowerState(std::string status);
        ApiErrorState CreateClient();
        ApiErrorState VerifyClientSession(std::string challenge);

    private:
        ApiErrorState HandleHttpRequestErrors(HttpResponse &response);
        bool HasError(int status_code);
        HttpResponse ApiHttpQuery(HttpRequest &request);
        const char *HttpMethodToString(HttpMethod method);
        std::string m_api_key_key;
        bool m_api_active;
    };
} // namespace Api
