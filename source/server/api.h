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

namespace Api
{
    /**
     * \brief Enum representing different API error states
     */
    enum ApiErrorState
    {
        API_NO_ERROR = 0,
        API_CLIENT_ERROR = 1,
        API_SERVER_ERROR = 2,
        API_ERROR_UNKNOWN = 999,
    };
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
            DELETE
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
            std::string body;
            std::string url = "https://v2.api.rigsofrods.org/";
            std::string headers;
            std::string user_agent = "Rigs of Rods Server";
            HttpMethod method = HttpMethod::GET;
            std::string content_type = "Content-Type: application/json";
        };

    public:
        Client();
        bool GetIpAddress(std::string &ip_addr);
        ApiErrorState CreateServer();
        ApiErrorState UpdateServer();
        ApiErrorState SyncServer();
        ApiErrorState SyncServerPowerState();
        ApiErrorState CreateClient();

    private:
        ApiErrorState HandleHttpRequestErrors(HttpResponse &response);
        bool HasError(int status_code);
        HttpRequest BuildHttpRequestQuery(HttpMethod method, std::string uri, std::string headers, std::string body);
        HttpResponse ApiHttpQuery(HttpRequest &request);
        const char *HttpMethodToString(HttpMethod method);
        std::string m_api_key;
        bool m_api_active;
    };
} // namespace Api
