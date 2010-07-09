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

#include "HttpMsg.h"

class UserAuth;
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
	char httpresp[65536];  //!< http response from the master server
	char trustlevel;       //!< the server's trustworthiness level, received upon registration
	char challenge[256];   //!< the server's token to identify itself to the master server
	bool exit;             //!< exit the server 
	bool wasregistered;    //!< is registered with the master server
	int error_count;       //!< counter how many failed heartbeats we had 
	HttpMsg resp;          //!< holds the latest response fromt he master server 

	bool registerServer(); //!< attempt to register with the master server
	bool sendHearbeat();   //!< send a heart beat message to the master server
	bool advertised;
	UserAuth *authresolver;
public:
    
	Notifier(UserAuth *u);
	
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
	
	std::string getChallenge() { return std::string(challenge); };
	bool getAdvertised() { return advertised; };
	int getTrustLevel() { return trustlevel; };
};
#endif
