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
#ifndef __Messaging_H__
#define __Messaging_H__

#include "SocketW.h"
#include "rornet.h"
#include "logger.h"

#ifdef __WIN32__
#include <time.h>
#endif

//TODO: does this even need to be a class? couldn't it be done just as well
//	using two functions outside of a class? 
class Messaging
{
public:
	Messaging(void) {;}
	~Messaging(void) {;}
	
	static int sendmessage(SWInetSocket *socket, int type, unsigned int  source, unsigned int len, char* content);
	static int receivemessage(SWInetSocket *socket, int *type, unsigned int *source, unsigned int *wrotelen, char* content, unsigned int bufferlen);

	static double getBandwitdthIncoming() { return bandwidthIncoming; };
	static double getBandwidthOutgoing() { return bandwidthOutgoing; };
	static double getBandwitdthIncomingRate() { return bandwidthIncomingRate; };
	static double getBandwidthOutgoingRate() { return bandwidthOutgoingRate; };

	static void updateMinuteStats();
	static int getTime() { return (int)time(NULL); };

protected:
	static double bandwidthIncoming;
	static double bandwidthOutgoing;
	static double bandwidthIncomingLastMinute;
	static double bandwidthOutgoingLastMinute;
	static double bandwidthIncomingRate;
	static double bandwidthOutgoingRate;
};


#endif

