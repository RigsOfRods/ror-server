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
#ifndef __Notifier_H__
#define __Notifier_H__

#include "SocketW.h"
#include "rornet.h"
#include "sequencer.h"

/**
 * @breif notifier class handles communications with the master server
 */
class Notifier
{
private:
	int lport;
	int maxclient;
	char* public_ip;
	char* server_name;
	char* terrain_name;
	Sequencer* sequencer;
	char httpresp[65536];
	char challenge[256];
	bool exit;
	bool passprotected;
	bool wasregistered;
	int servermode;
	int error_count;

	/// registration call to the master server to register this server
	bool registerServer();
	bool sendHearbeat();

public:
	Notifier(char* pubip, int port, Sequencer* seq, int max_client, char* servname, char* terrname, bool pwprotected, int servermode);
	~Notifier(void);
	void loop();
	int HTTPGET(char* URL);
	int HTTPPOST(char* URL, char* data);

	char *getResponse() { return httpresp; };

	/// call to the master server to unregister this server.
	bool unregisterServer();
};
#endif
