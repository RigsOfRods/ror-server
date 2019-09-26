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
            unsigned int payload_buf_len) {

        assert(out_type != nullptr);
        assert(out_source != nullptr);
        assert(out_stream_id != nullptr);
        assert(out_payload != nullptr);

        RoRnet::Header head;
        std::memset(out_payload, 0, payload_buf_len);
        size_t payload_size = 0;
        Result result = RecvAll(socket,
            (std::byte*)&head, sizeof(RoRnet::Header),
            (std::byte*)out_payload, payload_buf_len, &payload_size);

        if (result == Result::DISCONNECT) {
            Logger::Log(LOG_VERBOSE, "ReceiveMessage: disconnect while receiving header");
            return -1;
        }
        else if (result == Result::FAILURE) {
            Logger::Log(LOG_ERROR, "ReceiveMessage: socket failure while receiving header");
            return -2;
        }

        *out_type = head.command;
        *out_source = head.source;
        *out_payload_len = head.size;
        *out_stream_id = head.streamid;

        if ( head.size > payload_buf_len) {
            Logger::Log(LOG_ERROR, "ReceiveMessage(): payload too long: %d b (max. is %d b)", head.size,
                        payload_buf_len);
            return -3;
        }

        Logger::Log(LOG_VERBOSE, "ReceiveMessage(): payload size=%u, already got %u",
            head.size, payload_size);
        if (head.size > payload_size) {
            //read the rest         

            Result result = RecvAll(socket,
                (std::byte*)out_payload+payload_size, head.size-payload_size,
                nullptr, 0, nullptr);

            if (result == Result::DISCONNECT) {
                Logger::Log(LOG_VERBOSE, "ReceiveMessage: disconnect while receiving payload");
                return -1;
            }
            else if (result == Result::FAILURE) {
                Logger::Log(LOG_ERROR, "ReceiveMessage: socket failure while receiving payload");
                return -2;
            }
        }

        StatsAddIncoming(sizeof(RoRnet::Header) + head.size);
        return 0;
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

    Result RecvAll(kissnet::tcp_socket& socket,
        std::byte* dst_buffer, size_t dst_size,
        std::byte* overflow_buf, size_t overflow_cap, size_t* out_overflow_size)
    {
        kissnet::buffer<4000> buffer; // arbitrary
        size_t total_size = 0;
        while (total_size < dst_size)
        {
            // Wait for data part (blocking mode)            
            auto[recv_size, sock_state] = socket.recv(buffer);
            Logger::Log(LOG_VERBOSE, "RecvAll(): got %u bytes (%u total, %u requested) and status %d",
                recv_size, total_size, dst_size, (int)sock_state);

            if (sock_state == kissnet::socket_status::cleanly_disconnected)
            {
                return Result::DISCONNECT;
            }
            else if (sock_state != kissnet::socket_status::valid)
            {
                return Result::FAILURE;
            }

            // Buffer data up to capacity
            memcpy(dst_buffer + total_size, buffer.data(),
                std::min(recv_size, dst_size - total_size));
            total_size += recv_size;
        }

        // Check overflow
        if (overflow_buf && total_size > dst_size)
        {
            size_t overflow_size = std::min(overflow_cap, total_size - dst_size);
            Logger::Log(LOG_VERBOSE, "RecvAll(): overflow %u bytes (%u total, %u requested, %u buffer cap)",
                overflow_size, total_size, dst_size, overflow_cap);
            memcpy(overflow_buf, buffer.data() + dst_size, overflow_size);
            *out_overflow_size = overflow_size;
        }
        return Result::SUCCESS;
    }

} // namespace Messaging
