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

class SWInetSocket;

#include "sequencer.h"

//TODO: does this even need to be a class? couldn't it be done just as well
//	using two functions outside of a class? 
class Messaging
{
public:
    Messaging(void) {;}
    ~Messaging(void) {;}
    
    static int sendmessage(SWInetSocket *socket, int type, int source, unsigned int streamid, unsigned int len, const char* content);
    static int receivemessage(SWInetSocket *socket, int *type, int *source, unsigned int *streamid, unsigned int *wrotelen, char* content, unsigned int bufferlen);

    static int broadcastLAN();

    static void addBandwidthDropIncoming(int bytes);
    static void addBandwidthDropOutgoing(int bytes);
    static stream_traffic_t getTraffic();

    static void updateMinuteStats();
    static int getTime();

    static std::string retrievePublicIpFromServer();

protected:
    static stream_traffic_t traffic;
};
