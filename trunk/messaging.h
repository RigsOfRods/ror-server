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
#include <time.h>


#define LOG_DEBUG 0
#define LOG_WARN 1
#define LOG_ERROR 2

extern int loglevel;
extern "C" {
void logmsgf(int level, const char* format, ...);
}

//TODO: does this even need to be a class? couldn't it be done just as well
//	using two functions outside of a class? 
class Messaging
{
public:
	Messaging(void) {;}
	~Messaging(void) {;}
	
	static int sendmessage(SWInetSocket *socket, int type, unsigned int  source, unsigned int len, char* content);
	static int receivemessage(SWInetSocket *socket, int *type, unsigned int *source, unsigned int *wrotelen, char* content, unsigned int bufferlen);

};


#endif

