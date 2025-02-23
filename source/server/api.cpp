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
/// @file api.c
#include "api.h"
#include "logger.h"
#include "config.h"
#include "json/json.h"

#include <rornet.h>

#include <curl/curl.h>
#include <curl/easy.h>

size_t CurlStringWriteFunc(void *ptr, size_t size, size_t nmemb, std::string *data)
{
    data->append((char *)ptr, size * nmemb);
    return size * nmemb;
}

namespace Api
{
    Client::Client() {}

    /**
     * \brief Call the API to retrieve our server public IP.
     *
     * \param ip_addr
     */
    bool Client::GetPublicIp(std::string &ip_addr)
    {
        HttpResponse response;
        ApiErrorState error_code;

        HttpRequest request(HttpMethod::GET, "/ip");

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        if (error_code == API_NO_ERROR)
        {
            // OK, we got back a string that we can update the reference with ...
            ip_addr = response.body;
        };

        return ( error_code != API_NO_ERROR );
    }

    /**
     * \brief Call the API to determine if it's callable.
     * 
     * \return bool True or False if we can call the API.
     */
    bool Client::Callable()
    {
        HttpResponse response;
        ApiErrorState error_code;

        HttpRequest request(HttpMethod::GET, "/");

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return ( error_code != API_NO_ERROR );
    }

    /**
     * \brief This will check if both the server unique identifier and a
     *        unique server identifier. The unique identifier is something we
     *        update and the API key is manually configured by the owner.
     * 
     * \return bool True or False if we're authenticated againsted the API.
     */
    bool Client::Authenticated()
    {
        return true;
    }

    /**
     * \brief Call the API to register our server with the server list.
     *
     * \return ApiErrorState Error state from the API.
     */
    ApiErrorState Client::CreateServer()
    {
        HttpResponse response;
        ApiErrorState error_code;

        Json::Value data(Json::objectValue);
        data["name"] = Config::getServerName();
        data["ip"] = Config::getIPAddr();
        data["port"] = Config::getListenPort();
        data["version"] = RORNET_VERSION;
        data["description"] = "This is temp";
        data["max_clients"] = Config::getMaxClients();
        data["has_password"] = Config::isPublic();

        HttpRequest request(HttpMethod::POST, "/servers", data.asString());

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return error_code;
    }

    /**
     * \brief Call the API to update our server with the server list.
     *
     * \return ApiErrorState Error state from the API.
     */
    ApiErrorState Client::UpdateServer()
    {
        HttpResponse response;
        ApiErrorState error_code;    

        char url[300] = "";
        sprintf(url, "/servers/%d", 10000);

        Json::Value data(Json::objectValue);

        HttpRequest request(HttpMethod::UPDATE, url, data.asString());

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return error_code;
    }

    /**
     * \brief Call the API to sync server statuses with the server list.
     *
     * \return ApiErrorState Error state from the API.
     */
    ApiErrorState Client::SyncServer()
    {
        HttpResponse response;
        ApiErrorState error_code;

        Json::Value data(Json::objectValue);

        HttpRequest request(HttpMethod::PATCH, "/servers", data.asString());

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return error_code;
    }

    /**
     * \brief Call the API to sync the server power state with the server list.
     *
     * \param status The current power state to report to the server list.
     * \return ApiErrorState Error state from the API.
     */
    ApiErrorState Client::SyncServerPowerState(std::string status)
    {
        HttpResponse response;
        ApiErrorState error_code;

        // We should look into verifying the power status later...
        // If we make a weird request while in a state of limbo where the API
        // is unable to reach us, we need to avoid making that wasteful call.
        Json::Value data(Json::objectValue);
        data["power_status"] = status;

        HttpRequest request(HttpMethod::UPDATE, "/servers", data.asString());

        // When we send a power status of "online" we are telling the API that
        // we're ready to accept people and can be publicly displayed. Otherwise
        // we remain hidden.
        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return error_code;
    }

    /**
     * \brief Call the API to verify a challenge for a client.
     * 
     * \param challenge The challenge to us by the client.
     * \return ApiErrorState Error state of the API.
     */
    ApiErrorState Client::VerifyClientSession(std::string challenge)
    {
        HttpResponse response;
        ApiErrorState error_code;

        // Need to maybe look into whether or not C++20 has better string formatting?
        char url[300] = "";
        sprintf(url, "/auth/sessions/%s/verify", "ee1b920c-f815-4c9e-b5a2-b60db71dba88");

        // We don't actually know what is in the claims of the challenge, so
        // we'll wait for the API to return a pass or fail on them.
        Json::Value data(Json::objectValue);
        data["challenge"] = challenge.c_str(); // TODO: I really don't appreciate how funky this is ...
        HttpRequest request(HttpMethod::GET, url, data.toStyledString());

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return error_code;
    }

    /**
     * \brief Execute an API HTTP query.
     *
     * \param request The HTTP request to execute.
     * \return HttpResponse The HTTP response received from the API query.
     */
    Client::HttpResponse Client::ApiHttpQuery(HttpRequest &request)
    {
        HttpResponse response;
        CURLcode curl_result;

        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HttpMethodToString(request.method));
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

        if (!m_api_key_key.empty())
        {
            // If the API key is present, we need to send it even if the call we're
            // making does not require authentication.
            request.headers.push_back("Authorization: Bearer " + m_api_key_key);
        }

        // Let the API know we're prefering JSON or HTML to be sent back.
        request.headers.push_back("Accept: application/json");

        struct curl_slist *headers = nullptr;
        for (const auto &header : request.headers)
        {
            headers = curl_slist_append(headers, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
        curl_easy_setopt(curl, CURLOPT_USERAGENT, request.user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStringWriteFunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

        curl_result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);

        curl_easy_cleanup(curl);
        curl = nullptr;

        if (curl_result != CURLE_OK)
        {
            Logger::Log(LOG_ERROR, "curl error!");
        }

        return response;
    }

    /**
     * \brief Handles and returns the API error code status.
     *
     * \param response The HTTP response
     * \return ApiErrorState Error state from the API
     */
    ApiErrorState Client::HandleHttpRequestErrors(HttpResponse &response)
    {
        ApiErrorState error_code = API_UNKNOWN_ERROR; // the HTTP response code may end up being 0 when endpoint isn't configured.

        if (!this->HasError(response.status_code))
        {
            Logger::Log(LOG_INFO, "no http warning");
            error_code = API_NO_ERROR;
        }

        if (response.status_code >= 400 && response.status_code < 500)
        {
            Logger::Log(LOG_ERROR, "client error http");
            error_code = API_CLIENT_ERROR;
        }

        else if (response.status_code >= 500)
        {
            Logger::Log(LOG_ERROR, "server error http");
            error_code = API_SERVER_ERROR;
        }

        return error_code;
    }

    /**
     * \brief Checks whether the provided status code represents an error.
     *
     * \param status_code The status code to check.
     * \return True if there is an error, false otherwise.
     */
    bool Client::HasError(int status_code)
    {
        return status_code >= 300 || status_code < 200;
    }

    /**
     * \brief Returns a string representation of an HTTP method.
     *
     * \param HttpMethod The HTTP method.
     * \return The HTTP method as a string.
     */
    const char *Client::HttpMethodToString(HttpMethod method)
    {
        switch (method)
        {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::DELETE:
            return "DELETE";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        default:
            return "UNKNOWN";
        }
    }
}
