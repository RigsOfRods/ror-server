/*
This file is part of "Rigs of Rods Server" (Relay mode)
Copyright 2007 Pierre-Michel Ricordel
Contact: pricorde@rigsofrods.com
"Rigs of Rods Server" is distributed under the terms of the GNU General Public License.

"Rigs of Rods Server" is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.

"Rigs of Rods Server" is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdarg.h>
#include <time.h>
#include <string>
#include <errno.h>

#include "messaging.h"
#include "sequencer.h"
#include "rornet.h"
#include "logger.h"
#include "SocketW.h"
#include "config.h"


stream_traffic_t Messaging::traffic = {0,0,0,0,0,0,0,0,0,0,0,0};

void Messaging::updateMinuteStats()
{
    STACKLOG;
	// normal bandwidth
	traffic.bandwidthIncomingRate       = (traffic.bandwidthIncoming - traffic.bandwidthIncomingLastMinute) / 60;
	traffic.bandwidthIncomingLastMinute = traffic.bandwidthIncoming;
	traffic.bandwidthOutgoingRate       = (traffic.bandwidthOutgoing - traffic.bandwidthOutgoingLastMinute) / 60;
	traffic.bandwidthOutgoingLastMinute = traffic.bandwidthOutgoing;

	// dropped bandwidth
	traffic.bandwidthDropIncomingRate       = (traffic.bandwidthDropIncoming - traffic.bandwidthDropIncomingLastMinute) / 60;
	traffic.bandwidthDropIncomingLastMinute = traffic.bandwidthDropIncoming;
	traffic.bandwidthDropOutgoingRate       = (traffic.bandwidthDropOutgoing - traffic.bandwidthDropOutgoingLastMinute) / 60;
	traffic.bandwidthDropOutgoingLastMinute = traffic.bandwidthDropOutgoing;
}

/**
 * @param socket socket to communicate over
 * @param type a type... dunno
 * @param source who is sending the message
 * @param len length of the content being sent
 * @param content message to send
 * @return dunno
 */
int Messaging::sendmessage(SWInetSocket *socket, int type, int source, unsigned int streamid, unsigned int len, const char* content)
{
    STACKLOG;
    if( NULL == socket )
    {
    	Logger::log( LOG_ERROR, "UID: %d - attempt to send a messaage over a"
    			"null socket.", source );
    	return -3;
    }
    //SWInetSocket* socket = Sequencer::getSocket(source)
	SWBaseSocket::SWBaseError error;
	header_t head;

	const int msgsize = sizeof(header_t) + len;

	if(msgsize >= MAX_MESSAGE_LENGTH)
	{
    	Logger::log( LOG_ERROR, "UID: %d - attempt to send too long message", source);
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

#if 0
	// comment out since this severly bloats the server log
	char body[len*2+1];
	memset( body, 0,len*2+1);
	bodyblockashex(body, content, len);
	Logger::log( LOG_DEBUG, "sending message:  \t%d\t%d\t%d", type, source, len);
	Logger::log( LOG_DEBUG, "%s", body);
#endif
	while (rlen < msgsize)
	{
		int sendnum = socket->send( buffer + rlen, msgsize - rlen, &error );
		if (sendnum < 0 || error != SWBaseSocket::ok) 
		{
			Logger::log(LOG_ERROR, "send error -1: %s", error.get_error().c_str());
			return -1;
		}
		rlen += sendnum;
	}
	//Logger::log(LOG_DEBUG, "message of size %d sent to uid %d.", rlen, source);
	traffic.bandwidthOutgoing += msgsize;
	return 0;
}

void Messaging::addBandwidthDropIncoming(int bytes)
{
	traffic.bandwidthDropIncoming += bytes;
}

void Messaging::addBandwidthDropOutgoing(int bytes)
{
	traffic.bandwidthDropOutgoing += bytes;
}

/**
 * Name: receivemessage
 * @param *socket:
 * @param *type:
 * @param *source:
 * @param *wrotelen:
 * @param content:
 * @param bufferlen:
 * @return
 */
int Messaging::receivemessage(SWInetSocket *socket, int *type, int *source, unsigned int *streamid, unsigned int *wrotelen, char* content,
		unsigned int bufferlen)
{
    STACKLOG;

    if( NULL == source )
    {
    	Logger::log( LOG_ERROR, "source is null, no where to send it.");
    	return -1;
    }
    
    if( NULL == socket )
    {
    	Logger::log( LOG_ERROR, "attempt to receive a messaage over a"
    			"null socket." );
    	return -3;
    }
    
	SWBaseSocket::SWBaseError error;
	
	char buffer[MAX_MESSAGE_LENGTH];
	memset(buffer,0, MAX_MESSAGE_LENGTH);
	
	int hlen=0;
	while (hlen<(int)sizeof(header_t))
	{
		int recvnum=socket->recv(buffer+hlen, sizeof(header_t)-hlen, &error);
		if (recvnum < 0 || error!=SWBaseSocket::ok)
		{
			Logger::log(LOG_ERROR, "receive error -2: %s", error.get_error().c_str());
			// this also happens when the connection is canceled
			return -2;
		}
		hlen+=recvnum;
	}
	header_t head;
	memcpy(&head, buffer, sizeof(header_t));
	*type     = head.command;
	*source   = head.source;
	*wrotelen = head.size;
	*streamid = head.streamid;
	
	if((int)head.size >= MAX_MESSAGE_LENGTH)
	{
    	return -3;
	}

	if( head.size > 0)
	{
		if(!socket)
			return -3;
		
		//read the rest
		while (hlen < (int)sizeof(header_t) + (int)head.size)
		{
			int recvnum = socket->recv(buffer + hlen,
					(head.size+sizeof(header_t)) - hlen, &error);
			if (recvnum<0 || error!=SWBaseSocket::ok)
			{
				Logger::log(LOG_ERROR, "receive error -1: %s",
						error.get_error().c_str());
				return -1;
			}
			hlen += recvnum;
		}
	}
	
#if 0
	// comment out since this severly bloats teh server log
	char body[*wrotelen*2+1];
	memset( body, 0,*wrotelen*2+1);
	bodyblockashex(body, content, *wrotelen);
	Logger::log( LOG_DEBUG, "received  message:\t%d\t%d\t%d", *type, *source, *wrotelen);
	Logger::log( LOG_DEBUG, "%s", body);
#endif
	//Logger::log(LOG_DEBUG, "message of size %d received by uid %d.", hlen, *source);
	traffic.bandwidthIncoming += (int)sizeof(header_t)+(int)head.size;
	memcpy(content, buffer+sizeof(header_t), bufferlen);
	return 0;
}

stream_traffic_t Messaging::getTraffic() { return traffic; };

int Messaging::getTime() { return (int)time(NULL); };


int Messaging::broadcastLAN()
{
#ifdef WIN32
	// since socketw only abstracts TCP, we are on our own with UDP here :-/
	// the following code was only tested under windows

	int sockfd = -1;
	int on = 1;
	struct sockaddr_in sendaddr;
	memset(&sendaddr, 0, sizeof(sendaddr));
	struct sockaddr_in recvaddr;
	memset(&recvaddr, 0, sizeof(recvaddr));

	WSADATA        wsd;
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		Logger::log(LOG_ERROR, "error starting up winsock");
		return 1;
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		Logger::log(LOG_ERROR, "error creating socket for LAN broadcast: %s", strerror(errno));
		return 1;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof(on)) < 0)
	{	
		Logger::log(LOG_ERROR, "error setting socket options for LAN broadcast: %s", strerror(errno));
		return 2;
	}
	
	sendaddr.sin_family      = AF_INET;
	sendaddr.sin_port        = htons(LAN_BROADCAST_PORT+1);
	sendaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr *)&sendaddr, sizeof(sendaddr))  == SOCKET_ERROR)
	{
		Logger::log(LOG_ERROR, "error binding socket for LAN broadcast: %s", strerror(errno));
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
		Logger::log(LOG_ERROR, "error sending data over socket for LAN broadcast: %s", strerror(errno));
		return 4;
	}

	// and close the socket again
	closesocket(sockfd);
	
	Logger::log(LOG_DEBUG, "LAN broadcast successful");
#endif // WIN32	
	return 0;
}

