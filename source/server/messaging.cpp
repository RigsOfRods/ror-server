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
#include "config.h"
#include "http.h"
#include "prerequisites.h"
#include "UnicodeStrings.h"

#include <cstring>
#include <cstddef>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <array>
#include <mutex>

static stream_traffic_t s_traffic = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static std::mutex s_traffic_mutex;

namespace Messaging {

    void UpdateMinuteStats() {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);

        // normal bandwidth
        s_traffic.bandwidthIncomingRate = (s_traffic.bandwidthIncoming - s_traffic.bandwidthIncomingLastMinute) / 60;
        s_traffic.bandwidthIncomingLastMinute = s_traffic.bandwidthIncoming;
        s_traffic.bandwidthOutgoingRate = (s_traffic.bandwidthOutgoing - s_traffic.bandwidthOutgoingLastMinute) / 60;
        s_traffic.bandwidthOutgoingLastMinute = s_traffic.bandwidthOutgoing;

        // dropped bandwidth
        s_traffic.bandwidthDropIncomingRate =
                (s_traffic.bandwidthDropIncoming - s_traffic.bandwidthDropIncomingLastMinute) / 60;
        s_traffic.bandwidthDropIncomingLastMinute = s_traffic.bandwidthDropIncoming;
        s_traffic.bandwidthDropOutgoingRate =
                (s_traffic.bandwidthDropOutgoing - s_traffic.bandwidthDropOutgoingLastMinute) / 60;
        s_traffic.bandwidthDropOutgoingLastMinute = s_traffic.bandwidthDropOutgoing;
    }

    void StatsAddIncoming(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthIncoming += static_cast<double>(bytes);
    }

    void StatsAddOutgoing(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthOutgoing += static_cast<double>(bytes);
    }

    void StatsAddIncomingDrop(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthDropIncoming += static_cast<double>(bytes);
    }

    void StatsAddOutgoingDrop(int bytes) {
        std::unique_lock<std::mutex> lock(s_traffic_mutex);
        s_traffic.bandwidthDropOutgoing += static_cast<double>(bytes);
    }

    stream_traffic_t GetTrafficStats() {
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
    int SendMessage(kissnet::tcp_socket& socket, int type, int source, unsigned int streamid, unsigned int len,
                    const char *content) {

        RoRnet::Header head;

        const size_t msgsize = sizeof(RoRnet::Header) + len;

        if (msgsize >= RORNET_MAX_MESSAGE_LENGTH) {
            Logger::Log(LOG_ERROR, "UID: %d - attempt to send too long message", source);
            return -4;
        }

        memset(&head, 0, sizeof(RoRnet::Header));
        head.command = type;
        head.source = source;
        head.size = len;
        head.streamid = streamid;

        // construct buffer
        std::byte buffer[RORNET_MAX_MESSAGE_LENGTH];
        memset(buffer, 0, RORNET_MAX_MESSAGE_LENGTH);
        memcpy(buffer, (void *)&head, sizeof(RoRnet::Header));
        memcpy(buffer + sizeof(RoRnet::Header), content, len);

        try
        {
            auto [sent_len, sock_state] = socket.send(buffer, msgsize);
            if (sock_state == kissnet::socket_status::cleanly_disconnected)
            {
                return -2;
            }
            else if (sock_state != kissnet::socket_status::valid)
            {
                Logger::Log(LOG_ERROR, "send error - invalid socket");
                return -3;
            }
        }
        catch (std::exception& e)
        {
            Logger::Log(LOG_ERROR, "send error: %s", e.what());
            return -1;
        }

        StatsAddOutgoing(msgsize);
        return 0;
    }

/**
 * @param out_type        Message type, see RoRnet::RoRnet::MSG2_* macros in rornet.h
 * @param out_source      Magic. Value 5000 used by serverlist to check this server.
 * @return                0 on success, negative number on error.
 */
    int ReceiveMessage(
            kissnet::tcp_socket& socket,
            int *out_type,
            int *out_source,
            unsigned int *out_stream_id,
            unsigned int *out_payload_len,
            char *out_payload,
            unsigned int payload_buf_len)
    {

        assert(out_type != nullptr);
        assert(out_source != nullptr);
        assert(out_stream_id != nullptr);
        assert(out_payload != nullptr);

        try
        {
            RoRnet::Header header;
            size_t header_recv = 0;
            while (header_recv < sizeof(RoRnet::Header))
            {
                const auto [recv_len, sock_state] = socket.recv(
                    reinterpret_cast<std::byte*>(&header), sizeof(RoRnet::Header));

                if (sock_state != kissnet::socket_status::valid)
                {
                    throw kissnet::socket_status(sock_state);
                }
                header_recv += recv_len;
            }

            *out_type = header.command;
            *out_source = header.source;
            *out_payload_len = header.size;
            *out_stream_id = header.streamid;

            if (header.size > payload_buf_len)
            {
                Logger::Log(LOG_ERROR,
                    "ReceiveMessage(): payload too long: %u (buffer cap: %u)",
                    header.size, payload_buf_len);
                return -3;
            }

            std::memset(out_payload, 0, payload_buf_len);
            size_t payload_recv = 0;
            while (payload_recv < header.size)
            {
                const auto [recv_len, sock_state] = socket.recv(
                    reinterpret_cast<std::byte*>(out_payload) + payload_recv,
                    payload_buf_len - payload_recv);

                if (sock_state != kissnet::socket_status::valid)
                {
                    throw kissnet::socket_status(sock_state);
                }
                payload_recv += recv_len;
            }

            StatsAddIncoming(sizeof(RoRnet::Header) + header.size);
            return 0;
        }
        catch (kissnet::socket_status& sock_e)
        {
            if (sock_e == kissnet::socket_status::cleanly_disconnected)
            {
                Logger::Log(LOG_VERBOSE, "ReceiveMessage(): disconnect while receiving data");
                return -1;
            }
            else
            {
                Logger::Log(LOG_ERROR, "ReceiveMessage(): Invalid socket state: %d", static_cast<int>(sock_e));
                return -2;
            }
        }
    }

    int getTime() { return (int) time(NULL); }

    int broadcastLAN() {
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
        sendaddr.sin_port        = htons(RORNET_LAN_BROADCAST_PORT+1);
        sendaddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(sockfd, (struct sockaddr *)&sendaddr, sizeof(sendaddr))  == SOCKET_ERROR)
        {
            Logger::Log(LOG_ERROR, "error binding socket for LAN broadcast: %s", strerror(errno));
            return 3;
        }

        recvaddr.sin_family      = AF_INET;
        recvaddr.sin_port        = htons(RORNET_LAN_BROADCAST_PORT);
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
