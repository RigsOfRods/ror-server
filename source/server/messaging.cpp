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
along with "Rigs of Rods Server". 
If not, see <http://www.gnu.org/licenses/>.
*/

#include "messaging.h"

#include "sequencer.h"
#include "rornet.h"
#include "logger.h"
#include "SocketW.h"
#include "config.h"
#include "http.h"

#include <stdarg.h>
#include <time.h>
#include <string>
#include <errno.h>
#include <assert.h>

#include <mutex>

static stream_traffic_t s_traffic = {0,0,0,0,0,0, 0,0,0,0,0,0};
static std::mutex       s_traffic_mutex;

namespace Messaging {

void UpdateMinuteStats()
{
    std::unique_lock<std::mutex> lock(s_traffic_mutex);

    // normal bandwidth
    s_traffic.bandwidthIncomingRate       = (s_traffic.bandwidthIncoming - s_traffic.bandwidthIncomingLastMinute) / 60;
    s_traffic.bandwidthIncomingLastMinute = s_traffic.bandwidthIncoming;
    s_traffic.bandwidthOutgoingRate       = (s_traffic.bandwidthOutgoing - s_traffic.bandwidthOutgoingLastMinute) / 60;
    s_traffic.bandwidthOutgoingLastMinute = s_traffic.bandwidthOutgoing;

    // dropped bandwidth
    s_traffic.bandwidthDropIncomingRate       = (s_traffic.bandwidthDropIncoming - s_traffic.bandwidthDropIncomingLastMinute) / 60;
    s_traffic.bandwidthDropIncomingLastMinute = s_traffic.bandwidthDropIncoming;
    s_traffic.bandwidthDropOutgoingRate       = (s_traffic.bandwidthDropOutgoing - s_traffic.bandwidthDropOutgoingLastMinute) / 60;
    s_traffic.bandwidthDropOutgoingLastMinute = s_traffic.bandwidthDropOutgoing;
}

void StatsAddIncoming(int bytes)
{
    std::unique_lock<std::mutex> lock(s_traffic_mutex);
    s_traffic.bandwidthIncoming += static_cast<double>(bytes);
}

void StatsAddOutgoing(int bytes)
{
    std::unique_lock<std::mutex> lock(s_traffic_mutex);
    s_traffic.bandwidthOutgoing += static_cast<double>(bytes);
}

void StatsAddIncomingDrop(int bytes)
{
    std::unique_lock<std::mutex> lock(s_traffic_mutex);
    s_traffic.bandwidthDropIncoming += static_cast<double>(bytes);
}

void StatsAddOutgoingDrop(int bytes)
{
    std::unique_lock<std::mutex> lock(s_traffic_mutex);
    s_traffic.bandwidthDropOutgoing += static_cast<double>(bytes);
}

stream_traffic_t GetTrafficStats()
{
    std::unique_lock<std::mutex> lock(s_traffic_mutex);
    return s_traffic;
}

/**
 * @param socket  Socket to communicate over
 * @param type    Command ID
 * @param source  Source ID
 * @param len     Data length
 * @param content Payload
 * @return 0 on success
 */
int SendMessage(SWInetSocket *socket, int type, int source, unsigned int streamid, unsigned int len, const char* content)
{
    assert(socket != nullptr);

    SWBaseSocket::SWBaseError error;
    header_t head;

    const int msgsize = sizeof(header_t) + len;

    if(msgsize >= MAX_MESSAGE_LENGTH)
    {
        Logger::Log( LOG_ERROR, "UID: %d - attempt to send too long message", source);
        return -4;
    }

    char buffer[MAX_MESSAGE_LENGTH];
    

    int rlen = 0;
    
    memset(&head, 0, sizeof(header_t));
    head.command  = type;
    head.source   = source;
    head.size     = len;
    head.streamid = streamid;
    
    // construct buffer
    memset(buffer, 0, MAX_MESSAGE_LENGTH);
    memcpy(buffer, (char *)&head, sizeof(header_t));
    memcpy(buffer + sizeof(header_t), content, len);

    while (rlen < msgsize)
    {
        int sendnum = socket->send( buffer + rlen, msgsize - rlen, &error );
        if (sendnum < 0 || error != SWBaseSocket::ok) 
        {
            Logger::Log(LOG_ERROR, "send error -1: %s", error.get_error().c_str());
            return -1;
        }
        rlen += sendnum;
    }
    StatsAddOutgoing(msgsize);
    return 0;
}

/**
 * @param out_type        Message type, see MSG2_* macros in rornet.h
 * @param out_source      Magic. Value 5000 used by serverlist to check this server.
 * @return                0 on success, negative number on error.
 */
int ReceiveMessage(
        SWInetSocket *socket,
        int* out_type,
        int* out_source,
        unsigned int* out_stream_id,
        unsigned int* out_payload_len,
        char* out_payload,
        unsigned int payload_buf_len)
{
    assert(socket        != nullptr);
    assert(out_type      != nullptr);
    assert(out_source    != nullptr);
    assert(out_stream_id != nullptr);
    assert(out_payload   != nullptr);

    char buffer[MAX_MESSAGE_LENGTH] = {};
    
    int hlen=0;
    SWBaseSocket::SWBaseError error;
    while (hlen<(int)sizeof(header_t))
    {
        int recvnum=socket->recv(buffer+hlen, sizeof(header_t)-hlen, &error);
        if (recvnum < 0 || error!=SWBaseSocket::ok)
        {
            Logger::Log(LOG_ERROR, "receive error -2: %s", error.get_error().c_str());
            // this also happens when the connection is canceled
            return -2;
        }
        hlen+=recvnum;
    }

    header_t head;
    memcpy(&head, buffer, sizeof(header_t));
    *out_type         = head.command;
    *out_source       = head.source;
    *out_payload_len  = head.size;
    *out_stream_id    = head.streamid;
    
    if((int)head.size >= MAX_MESSAGE_LENGTH)
    {
        Logger::Log(LOG_ERROR, "ReceiveMessage(): payload too long: %d b (max. is %d b)", head.size, MAX_MESSAGE_LENGTH);
        return -3;
    }

    if( head.size > 0)
    {
        //read the rest
        while (hlen < (int)sizeof(header_t) + (int)head.size)
        {
            int recvnum = socket->recv(buffer + hlen,
                    (head.size+sizeof(header_t)) - hlen, &error);
            if (recvnum<0 || error!=SWBaseSocket::ok)
            {
                Logger::Log(LOG_ERROR, "receive error -1: %s",
                        error.get_error().c_str());
                return -1;
            }
            hlen += recvnum;
        }
    }

    StatsAddIncoming((int)sizeof(header_t)+(int)head.size);
    memcpy(out_payload, buffer+sizeof(header_t), payload_buf_len);
    return 0;
}

int getTime() { return (int)time(NULL); }

int broadcastLAN()
{
#ifdef _WIN32
    // since socketw only abstracts TCP, we are on our own with UDP here :-/
    // the following code was only tested under windows

    int sockfd = -1;
    int on = 1;
    struct sockaddr_in sendaddr;
    memset(&sendaddr, 0, sizeof(sendaddr));
    struct sockaddr_in recvaddr;
    memset(&recvaddr, 0, sizeof(recvaddr));

    WSADATA wsd;
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        Logger::Log(LOG_ERROR, "error starting up winsock");
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        Logger::Log(LOG_ERROR, "error creating socket for LAN broadcast: %s", strerror(errno));
        return 1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof(on)) < 0)
    {	
        Logger::Log(LOG_ERROR, "error setting socket options for LAN broadcast: %s", strerror(errno));
        return 2;
    }
    
    sendaddr.sin_family      = AF_INET;
    sendaddr.sin_port        = htons(LAN_BROADCAST_PORT+1);
    sendaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *)&sendaddr, sizeof(sendaddr))  == SOCKET_ERROR)
    {
        Logger::Log(LOG_ERROR, "error binding socket for LAN broadcast: %s", strerror(errno));
        return 3;
    }

    recvaddr.sin_family      = AF_INET;
    recvaddr.sin_port        = htons(LAN_BROADCAST_PORT);
    recvaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);


    // construct the message
    char tmp[1024] = "";
    memset(tmp, 0, 1023);
    // format:
    // RoRServer|Protocol V |IP           :PORT |terrain name   |password protected?
    // RoRServer|RoRnet_2.35|192.168.0.235:12001|myterrain.terrn|0
    sprintf(tmp, "RoRServer|%s|%s:%d|%s|%d", 
        RORNET_VERSION, 
        Config::getIPAddr().c_str(),
        Config::getListenPort(),
        Config::getTerrainName().c_str(),
        Config::getPublicPassword() == "" ? 0 : 1
        );

    // send the message
    int numbytes = 0;
    while((numbytes = sendto(sockfd, tmp, strnlen(tmp, 1024), 0, (struct sockaddr *)&recvaddr, sizeof recvaddr)) < -1) 
    {
        Logger::Log(LOG_ERROR, "error sending data over socket for LAN broadcast: %s", strerror(errno));
        return 4;
    }

    // and close the socket again
    closesocket(sockfd);
    
    Logger::Log(LOG_DEBUG, "LAN broadcast successful");
#endif // _WIN32	
    return 0;
}

} // namespace Messaging
