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
#include "messaging.h"
#include <stdarg.h>


double Messaging::bandwidthIncoming=0;
double Messaging::bandwidthOutgoing=0;
double Messaging::bandwidthIncomingLastMinute=0;
double Messaging::bandwidthOutgoingLastMinute=0;
double Messaging::bandwidthIncomingRate=0;
double Messaging::bandwidthOutgoingRate=0;

void Messaging::updateMinuteStats()
{
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
int Messaging::sendmessage(SWInetSocket *socket, int type, unsigned int source, unsigned int len, char* content)
{
	SWBaseSocket::SWBaseError error;
	header_t head;
	memset(&head, 0, sizeof(header_t));
	head.command=type;
	head.source=source;
	head.size=len;
	int hlen=0;
	
	// construct buffer
	const int msgsize = sizeof(header_t) + len;
	char buffer[MAX_MESSAGE_LENGTH];
	memset(buffer, 0, MAX_MESSAGE_LENGTH);
	memcpy(buffer, (char *)&head, sizeof(header_t));
	memcpy(buffer+sizeof(header_t), content, len);

	int rlen=0;
	while (rlen<(int)msgsize)
	{
		int sendnum=socket->send(buffer+rlen, msgsize-rlen, &error);
		if (sendnum<0 || error!=SWBaseSocket::ok) 
		{
			return -1;
		}
		rlen+=sendnum;
	}
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
int Messaging::receivemessage(SWInetSocket *socket, int *type, unsigned int *source, unsigned int *wrotelen, char* content, unsigned int bufferlen)
{
	SWBaseSocket::SWBaseError error;
	
	char buffer[MAX_MESSAGE_LENGTH];
	memset(buffer,0, MAX_MESSAGE_LENGTH);
	
	int hlen=0;
	while (hlen<(int)sizeof(header_t))
	{
		int recvnum=socket->recv(buffer+hlen, sizeof(header_t)-hlen,&error);
		if (recvnum<0 || error!=SWBaseSocket::ok)
		{
			// this also happens when the connection is canceled
			return -1;
		}
		hlen+=recvnum;
	}
	header_t head;
	memcpy(&head, buffer, sizeof(header_t));
	*type=head.command;
	*source=head.source;
	*wrotelen=head.size;
	if(head.size>0)
	{
		//read the rest
		while (hlen<(int)sizeof(header_t)+(int)head.size)
		{
			int recvnum=socket->recv(buffer+hlen, (head.size+sizeof(header_t))-hlen,&error);
			if (recvnum<0 || error!=SWBaseSocket::ok)
			{
				return -1;
			}
			hlen+=recvnum;
		}
	}
	bandwidthIncoming += (int)sizeof(header_t)+(int)head.size;
	memcpy(content, buffer+sizeof(header_t), bufferlen);
	return 0;
}

int loglevel=1;

extern "C" {

void logmsgf(int level, const char* format, ...)
{
	if (level<loglevel) return;
	time_t lotime=time(NULL);
	char timestr[50];
#ifdef __WIN32__
	ctime_s(timestr, 50, &lotime);
#else
	ctime_r(&lotime, timestr);
#endif
	timestr[strlen(timestr)-1]=0;
	printf("%s: ", timestr);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}

}	//	extern "C"

