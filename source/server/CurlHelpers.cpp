/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2023 Petr Ohlidal

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

#ifdef WITH_CURL

#include "CurlHelpers.h"
#include "ScriptEngine.h"

#include <curl/curl.h>
#include <curl/easy.h>

#include <string>

static size_t CurlStringWriteFunc(void *ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

static size_t CurlXferInfoFunc(void* ptr, curl_off_t filesize_B, curl_off_t downloaded_B, curl_off_t uploadsize_B, curl_off_t uploaded_B)
{
    CurlTaskContext* context = (CurlTaskContext*)ptr;

    context->ctc_script_engine->curlStatus(
        CURL_STATUS_PROGRESS, (int)downloaded_B, (int)filesize_B, context->ctc_displayname, "");

    // If you don't return 0, the transfer will be aborted - see the documentation
    return 0;
}

bool GetUrlAsString(const std::string& url, const std::vector<std::string>& headers, CURLcode& curl_result, long& response_code, std::string& response_payload)
{
    std::string response_header;
    std::string user_agent = "Rigs of Rods Server";

    struct curl_slist* slist;
    slist = NULL;
    for (const std::string& header : headers)
    {
        slist = curl_slist_append(slist, header.c_str());
    }

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#ifdef _WIN32
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif // _WIN32
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlStringWriteFunc);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CurlXferInfoFunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_payload);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);

    curl_result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    curl = nullptr;

    return curl_result == CURLE_OK && response_code == 200;
}

bool CurlRequestThreadFunc(CurlTaskContext context)
{
    context.ctc_script_engine->curlStatus(CURL_STATUS_START, 0, 0, context.ctc_displayname, "");
    std::string data;
    CURLcode curl_result = CURLE_OK;
    long http_response = 0;
    if (GetUrlAsString(context.ctc_url, context.ctc_headers, /*out:*/curl_result, /*out:*/http_response, /*out:*/data))
    {
        context.ctc_script_engine->curlStatus(CURL_STATUS_SUCCESS, (int)curl_result, (int)http_response, context.ctc_displayname, data);
        return true;
    }
    else
    {
        context.ctc_script_engine->curlStatus(CURL_STATUS_FAILURE, (int)curl_result, (int)http_response, context.ctc_displayname, curl_easy_strerror(curl_result));
        return false;
    }
}

#endif // WITH_CURL
