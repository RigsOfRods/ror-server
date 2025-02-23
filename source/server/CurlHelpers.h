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

enum CurlStatusType
{
    CURL_STATUS_INVALID,  //!< Should never be reported.
    CURL_STATUS_START,    //!< New CURL request started, n1/n2 both 0.
    CURL_STATUS_PROGRESS, //!< Download in progress, n1 = bytes downloaded, n2 = total bytes.
    CURL_STATUS_SUCCESS,  //!< CURL request finished, n1 = CURL return code, n2 = HTTP result code, message = received payload.
    CURL_STATUS_FAILURE,  //!< CURL request finished, n1 = CURL return code, n2 = HTTP result code, message = CURL error string.
};

#ifdef WITH_CURL

class ScriptEngine;

#include <curl/curl.h>
#include <curl/easy.h>

#include <string>
#include <vector>

struct CurlTaskContext
{
    std::string ctc_displayname;
    std::string ctc_url;
    std::vector<std::string> ctc_headers;
    ScriptEngine* ctc_script_engine;
    // Status is reported via new server callback `curlStatus()`
};

bool GetUrlAsString(const std::string& url, const std::vector<std::string>& headers, CURLcode& curl_result, long& response_code, std::string& response_payload);

bool CurlRequestThreadFunc(CurlTaskContext task);



#endif // WITH_CURL
