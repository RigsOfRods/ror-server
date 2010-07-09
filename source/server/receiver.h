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
#ifndef __Receiver_H__
#define __Receiver_H__

#include <pthread.h>
#include "rornet.h"
class SWInetSocket;

class Receiver
{
private:
	pthread_t thread;
	int id;
	SWInetSocket *sock;
	char dbuffer[MAX_MESSAGE_LENGTH];
	bool running;
public:
	Receiver();
	~Receiver(void);
	void reset(int pos, SWInetSocket *socky);
	void stop();
	void threadstart();
};
#endif
