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

#include "notifier.h"

#include "SocketW.h"
#include "sequencer.h"
#include "rornet.h"
#include "messaging.h"
#include "logger.h"
#include "config.h"

#include <stdexcept>
#include <cstdio>
#ifdef __GNUC__
#include <unistd.h>
#include <stdlib.h>
#endif

Notifier::Notifier(Sequencer* sequencer):
    trustlevel(0),
    exit(false),
    wasregistered(false),
    error_count(0),
    advertised(false),
    is_active(false),
    m_sequencer(sequencer)
{
    memset( &httpresp, 0, 65536);
    memset( &challenge, 0, 256);
}

Notifier::~Notifier(void)
{
    exit = true;
}

void Notifier::activate()
{
    is_active = true;
}

void Notifier::deactivate()
{
    is_active = false;
}

bool Notifier::registerServer()
{
    char regurl[1024];
    int responseformat = 2;
    sprintf(regurl, "%s/register/?name=%s&description=%s&ip=%s&port=%i&"
            "terrainname=%s&maxclients=%i&version=%s&pw=%d&format=%d", 
        REPO_URLPREFIX,
        Config::getServerName().c_str(),
        "",
        Config::getIPAddr().c_str(), 
        Config::getListenPort(),
        Config::getTerrainName().c_str(),
        Config::getMaxClients(),
        RORNET_VERSION,
        Config::isPublic(),
        responseformat);
    
    // format = 2 will result in different response on registration format.
    Logger::Log(LOG_DEBUG, "%s%s", "sending registration request: ", regurl);
    Logger::Log(LOG_INFO, "Trying to register at Master Server ... (this can take some "
            "seconds as your server is being checked by the Master server)");
    if (HTTPGET(regurl) < 0)
        return false;


    if(responseformat==1)
    {
        // old format
        std::string body = getResponse().GetBody();
        if(body.find("error") != std::string::npos || body.length() < 40)
        {
            // print out the error.
            Logger::Log(LOG_ERROR, "got that as registration response: %s", body.c_str());
            Logger::Log(LOG_ERROR, "!!! Server is NOT registered at the Master server !!!");
            wasregistered=false;
            return false;
        }
        else
        {
            Logger::Log(LOG_DEBUG, "got that as registration response: %s", body.c_str());
            
            memset(&challenge, 0, 40);
            strncpy( challenge, body.c_str(), 40 );
            Logger::Log(LOG_INFO,"Server is registered at the Master server.");
            wasregistered=true;
            return true;
        }
    } else if (responseformat == 2)
    {
        // new format
        std::vector<std::string> lines = getResponse().GetBodyLines();
        if(lines.size() < 2)
        {
            Logger::Log(LOG_ERROR, "got wrong server response upon registration: only %d lines instead of minimal 2", lines.size());
            wasregistered=false;
            return false;
        }
        
        // zero line = response to registration, 'ok' or 'error'
        std::string status_short = lines[0];
        // status message - an error message
        std::string status_long = lines[1];
    
        if(status_short == "ok")
        {
        }else if(status_short == "error")
        {
            Logger::Log(LOG_ERROR, "error upon registration: %s: %s", status_short.c_str(), status_long.c_str());
            Logger::Log(LOG_ERROR, "!!! Server is NOT registered at the Master server !!!");
            wasregistered=false;
            return false;
        }

        // server challenge
        std::string challenge_response = lines[2].c_str();
        // server trustness level
        if(lines.size() > 2)
            trustlevel = atoi(lines[3].c_str());

        Logger::Log(LOG_DEBUG, "%s: %s", status_short.c_str(), status_long.c_str());
        Logger::Log(LOG_DEBUG, "this server got trustlevel %d", trustlevel);
        
        //copy data
        memset(&challenge, 0, 40);
        strncpy( challenge, challenge_response.c_str(), 40 );
        Logger::Log(LOG_INFO,"Server is registered at the Master server.");
        wasregistered=true;
        advertised = true;
        return true;
    }
    return false;
}

bool Notifier::unregisterServer()
{
    if(!wasregistered || !advertised)
        return false;
    char unregurl[1024];
    sprintf(unregurl, "%s/unregister/?challenge=%s", REPO_URLPREFIX, challenge);
    if (HTTPGET(unregurl) < 0)
        return false;
    advertised = false;

    return getResponse().GetBody() == "ok";
}

bool Notifier::sendHearbeat()
{
    char hearbeaturl[1024] = "";
    char hearbeatdata[16384] = "";
    memset(hearbeatdata, 0, 16384);
    sprintf(hearbeaturl, "%s/heartbeat/", REPO_URLPREFIX);
    if(m_sequencer->getHeartbeatData(challenge, hearbeatdata))
        return false;

    Logger::Log(LOG_DEBUG, "heartbeat data (%d bytes long) sent to master server: >>%s<<", strnlen(hearbeatdata, 16384), hearbeatdata);
    if (HTTPPOST(hearbeaturl, hearbeatdata) < 0)
        return false;
    // the server gives back "failed" or "ok"	
    return getResponse().GetBody() == "ok";
}

void Notifier::loop()
{
    if (!advertised && Config::getServerMode() == SERVER_AUTO)
    {
        Logger::Log(LOG_WARN, "using LAN mode, probably no internet users will "
                "be able to join your server!");
    }
    else if (!advertised && Config::getServerMode() == SERVER_INET)
    {
        Logger::Log(LOG_ERROR, "registration failed, exiting!");
        return;
    }

    //heartbeat
    while (!exit)
    {
        // update some statistics (handy to use in here, as we have a minute-timer basically)
        Messaging::updateMinuteStats();
        m_sequencer->updateMinuteStats();
        //Sequencer::printStats();

        //every minute
#ifndef _WIN32
        sleep(60);
#else
        Sleep(60*1000);
#endif

        if(advertised && !exit)
        {
            bool result = sendHearbeat();
            if (result) error_count=0;
            if(!result && Config::getServerMode() == SERVER_INET)
            {
                error_count++;
                if (error_count==5) 
                {
                    Logger::Log(LOG_ERROR,"heartbeat failed, exiting!");
                    break;
                }
                else
                {
                    Logger::Log(LOG_WARN,"heartbeat failed, will try again");
                }
            }else if(!result && Config::getServerMode() != SERVER_INET)
                Logger::Log(LOG_ERROR,"heartbeat failed!");
        }
    }

    unregisterServer();
}

int Notifier::HTTPGET(const char* URL)
{
    int res=0;
    SWBaseSocket::SWBaseError error;
    SWInetSocket mySocket;

    if (mySocket.connect(80, REPO_SERVER, &error))
    {
        char query[2048];
        sprintf(query, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", URL, REPO_SERVER);
        Logger::Log(LOG_DEBUG,"Query to Master server: %s", query);
        if (mySocket.sendmsg(query, &error)<0)
        {
            Logger::Log(LOG_ERROR,"Notifier: could not send request (%s)", error.get_error().c_str());
            res=-1;
        }
        int rlen=mySocket.recv(httpresp, 65536, &error);
        if (rlen>0 && rlen<65535) httpresp[rlen]=0;
        else
        {
            Logger::Log(LOG_ERROR,"Notifier: could not get a valid response (%s)", error.get_error().c_str());
            res=-1;
        }
        Logger::Log(LOG_DEBUG,"Response from Master server:'%s'", httpresp);
        try
        {
            std::string response_str(httpresp);
            resp.FromBuffer(response_str);
        }
        catch( std::runtime_error e)
        {
            Logger::Log(LOG_ERROR, e.what() );
            res = -1;
        }
        // disconnect
        mySocket.disconnect();
    }
    else
    {
        Logger::Log(LOG_ERROR,"Notifier: could not connect to the Master server (%s)", error.get_error().c_str());
        res=-1;
    }
    return res;
}

int Notifier::HTTPPOST(const char* URL, const char* data)
{
    int res=0;
    SWBaseSocket::SWBaseError error;
    SWInetSocket mySocket;

    if (mySocket.connect(80, REPO_SERVER, &error))
    {
        char query[2048];
        int len = (int)strnlen(data, 16000);
        sprintf(query, "POST %s HTTP/1.0\r\n" \
                       "Host: %s\r\n" \
                       "Content-Type: application/octet-stream\r\n" \
                       "Content-Length: %d\r\n"
                       "\r\n" \
                       "%s", \
                       URL, REPO_SERVER, len, data);
        Logger::Log(LOG_DEBUG,"Query to Master server: %s", query);
        if (mySocket.sendmsg(query, &error)<0)
        {
            Logger::Log(LOG_ERROR,"Notifier: could not send request (%s)", error.get_error().c_str());
            res=-1;
        }
        int rlen=mySocket.recv(httpresp, 65536, &error);
        
        
        if (rlen>0 && rlen<65535) httpresp[rlen]=0;
        else
        {
            Logger::Log(LOG_ERROR,"Notifier: could not get a valid response (%s)",
                    error.get_error().c_str());
            res=-1;
        }
        Logger::Log(LOG_DEBUG,"Response from Master server:'%s'", httpresp);
        try
        {
            resp.FromBuffer(std::string(httpresp));
        }
        catch( std::runtime_error e)
        {
            Logger::Log(LOG_ERROR, e.what() );
            res = -1;
        }
        
        // disconnect
        mySocket.disconnect();
    }
    else
    {
        Logger::Log(LOG_ERROR,"Notifier: could not connect to the Master server (%s)", error.get_error().c_str());
        res=-1;
    }
    return res;
}
