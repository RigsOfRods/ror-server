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
        return true;
    }

    /**
     * \brief Call the API to determine if it's callable.
     * 
     * \return bool True or False if we can call the API.
     */
    bool Client::Callable()
    {
        return true;
    }

    /**
     * \brief Check whether we've already authenticated against the API.
     *        This is typically done to ensure we don't call CreateServer()
     *        again. This condition is satsified if an API key and server
     *        unique identifier is present.
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
        HttpRequest request;
        HttpResponse response;
        ApiErrorState error_code;

        char url[300] = "";
        sprintf(url, "%s/servers", Config::GetServerlistHostC());

        Json::Value data(Json::objectValue);
        data["name"] = Config::getServerName();
        data["ip"] = Config::getIPAddr();
        data["port"] = Config::getListenPort();
        data["version"] = RORNET_VERSION;
        data["description"] = "This is temp";
        data["max_clients"] = Config::getMaxClients();
        data["has_password"] = Config::isPublic();

        request = this->BuildHttpRequestQuery(HttpMethod::POST,
                                              "/servers", 
                                              "", 
                                              data.asString());

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
        HttpRequest request;
        HttpResponse response;
        ApiErrorState error_code;    

        char url[300] = "";
        sprintf(url, "%s/servers/%d", Config::GetServerlistHostC(), 2);

        Json::Value data(Json::objectValue);

        request = this->BuildHttpRequestQuery(HttpMethod::PUT,
                                              url,
                                              "", 
                                              data.asString());

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
        HttpRequest request;
        HttpResponse response;
        ApiErrorState error_code;

        // Verify the power status of the server later...
        char url[300] = "";
        sprintf(url, "%s/servers/%d/sync", Config::GetServerlistHostC(), 2);

        request = this->BuildHttpRequestQuery(HttpMethod::PATCH,
                                              url,
                                              "",
                                              "");

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
        HttpRequest request;
        HttpResponse response;
        ApiErrorState error_code;

        // We should look into verifying the power status later...
        // If we make a weird request while in a state of limbo where the API
        // is unable to reach us, we need to avoid making that wasteful call.
        Json::Value data(Json::objectValue);
        data["power_status"] = status;
        request.body = data.asString();

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
        HttpRequest request;
        HttpResponse response;
        ApiErrorState error_code;

        // Need to maybe look into whether or not C++20 has better string formatting?
        char url[300] = "";
        sprintf(url, "%s/auth/session/%d/verify", Config::GetServerlistHostC(), 2);

        // We don't actually know what is in the claims of the challenge, so
        // we'll wait for the API to return a pass or fail on them.
        Json::Value data(Json::objectValue);
        data["challenge"] = challenge;
        request = this->BuildHttpRequestQuery(HttpMethod::GET,
                                              url,
                                              "", 
                                              data.asString());

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

        Logger::Log(LOG_INFO, "");
        Logger::Log(LOG_DEBUG, "");

        if (curl_result != CURLE_OK)
        {
            Logger::Log(LOG_ERROR, "");
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
        ApiErrorState error_code;
        LogLevel log_level;

        if (!this->HasError(response.status_code))
        {
            error_code = API_NO_ERROR;
        }

        if (response.status_code >= 400 && response.status_code < 500)
        {
            error_code = API_CLIENT_ERROR;
        }

        else if (response.status_code >= 500)
        {
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
     * \brief Build the HTTP request query.
     * 
     * \param method The HTTP method.
     * \param uri The URI of the request.
     * \param headers The headers of the request.
     * \param body The body of the request.
     * \return HttpRequest The HTTP request.
     */
    Client::HttpRequest Client::BuildHttpRequestQuery(HttpMethod method, std::string uri, std::string headers, std::string body)
    {
        HttpRequest request;
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
