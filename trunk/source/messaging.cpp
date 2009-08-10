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

#include "messaging.h"
#include "sequencer.h"
#include "rornet.h"
#include "logger.h"
#include "SocketW.h"


double Messaging::bandwidthIncoming=0;
double Messaging::bandwidthOutgoing=0;
double Messaging::bandwidthIncomingLastMinute=0;
double Messaging::bandwidthOutgoingLastMinute=0;
double Messaging::bandwidthIncomingRate=0;
double Messaging::bandwidthOutgoingRate=0;

#if 0
// asHex MUST be 2x as long as body
static void bodyblockashex(char* asHex, const char* body, unsigned int len)
{
	char tmp[20];
	unsigned int i=0;
	while(i<len)
	{
		memset(tmp, 0, 20);
		unsigned char d = (unsigned char)*body;
		sprintf(tmp, "%x", d);
		strcat(asHex, tmp);
		body++;
		i++;
	}
}
#endif

void Messaging::updateMinuteStats()
{
    STACKLOG;
	bandwidthIncomingRate = (bandwidthIncoming-bandwidthIncomingLastMinute)/60;
	bandwidthIncomingLastMinute = bandwidthIncoming;
	bandwidthOutgoingRate = (bandwidthOutgoing-bandwidthOutgoingLastMinute)/60;
	bandwidthOutgoingLastMinute = bandwidthOutgoing;
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
	char buffer[MAX_MESSAGE_LENGTH];
	

	int rlen = 0;
	
	memset(&head, 0, sizeof(header_t));
	head.command  = type;
	head.source   = source;
	head.size     = len;
	head.streamid = len;
	
	// construct buffer
	memset(buffer, 0, MAX_MESSAGE_LENGTH);
	memcpy(buffer, (char *)&head, sizeof(header_t));
	memcpy(buffer + sizeof(header_t), content, len);

#if 0
	// comment out since this severly bloats teh server log
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
	bandwidthOutgoing += msgsize;
	return 0;
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
	bandwidthIncoming += (int)sizeof(header_t)+(int)head.size;
	memcpy(content, buffer+sizeof(header_t), bufferlen);
	return 0;
}

double Messaging::getBandwitdthIncoming() { return bandwidthIncoming; };
double Messaging::getBandwidthOutgoing() { return bandwidthOutgoing; };
double Messaging::getBandwitdthIncomingRate() { return bandwidthIncomingRate; };
double Messaging::getBandwidthOutgoingRate() { return bandwidthOutgoingRate; };

int Messaging::getTime() { return (int)time(NULL); };


