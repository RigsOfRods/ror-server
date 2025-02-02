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
#include "json/json.h"

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
     * \brief Call the API to retrieve our server public IP address.
     *
     * \param ip_addr
     */
    bool Client::GetIpAddress(std::string &ip_addr)
    {
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

        Json::Value data(Json::objectValue);
        data["name"];
        data["ip"];
        data["port"];
        data["version"];
        data["description"];
        data["max_clients"];
        data["has_password"];

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

        Json::Value data(Json::objectValue);
        request.body = data.asString();

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

        Json::Value data(Json::objectValue);
        request.method = HttpMethod::POST;
        request.body = data.asString();

        response = this->ApiHttpQuery(request);
        error_code = this->HandleHttpRequestErrors(response);

        return error_code;
    }

    /**
     * \brief Call the API to sync the server power state with the server list.
     *
     * \return ApiErrorState Error state from the API.
     */
    ApiErrorState Client::SyncServerPowerState()
    {
        HttpRequest request;
        HttpResponse response;
        ApiErrorState error_code;

        Json::Value data(Json::objectValue);
        request.body = data.asString();

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
