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
#include "HttpMsg.h"

/**
 * The notifier class communicated with the master server, it is called from
 * the sequencer class via Sequencer::notifyRoutine. The loop method is
 * essentially the main loop for the server.
 * 
 * @brief notifier class handles communications with the master server
 */
class Notifier
{
private:
	int lport;             //!< port to listen to for incoming connections
	int maxclient;         //!< maximum number of clients this server can support
	char* public_ip;       //!< public ip address of the server
	char* server_name;     //!< name of the server
	char* terrain_name;    //!< name of the terrain
	char httpresp[65536];  //!< http response from the master server
	char challenge[256];   //!< ??
	bool exit;             //!< exit the server
	bool passprotected;    //!< password protection for logging into this server 
	bool wasregistered;    //!< is registered with the master server
	bool rconenabled;      //!< check is the remote console is eneabled
	int servermode;        //!< Internet or LAN mode 
	int error_count;       //!< ??
	HttpMsg resp;          //!< holds the latest response fromt he master server 

	bool registerServer(); //!< attempt to register with the master server
	bool sendHearbeat();   //!< send a heart beat message to the master server

public:
    
	Notifier(char* pubip,
	        int port,
	        int max_client,
	        char* servname,
	        char* terrname,
	        bool pwprotected,
	        int servermode,
	        bool rconenabled);
	
	~Notifier(void);
	
	//! main loop for the server?
	void loop();
	
	/**
	 * perform an HTTPGET
     * @param[in] master server address
     * @return the amount of data received, if negative an error occured
	 */
	int HTTPGET(const char* URL);

    /**
     * perform an HTTPPOST
     * @param[in] master server address
     * @param[in] data the message to send
     * @return the amount of data received, if negative an error occured
     */
	int HTTPPOST(const char* URL, const char* data);

	/**
	 * @return the respond from the last HTTP{GET,POST} method call
	 */
	HttpMsg getResponse() { return resp; }

	//! notify the master server that this server is shutting down 
	bool unregisterServer();
};
#endif
