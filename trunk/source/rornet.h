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
#ifndef __RoRnet_H__
#define __RoRnet_H__

#define SERVERPORT 12333
#define CLIENTPORT 12334

#define MAX_PEERS 64
//#define MAX_MLEN 1024
#define MAX_MESSAGE_LENGTH 8192

#define RORNET_VERSION "RoRnet_2.1"

#define MSG_REGISTER 0      //!< client registers with magic!
#define MSG_PING 1          //!< heartbeat sent by server ()
#define MSG_PING_RESP 2     //!< heartbeat client response ()
#define MSG_SIGNAL 3        //!< signals a new client
#define MSG_ZOMBIE 4        //!< signals a zombie client
#define MSG_GET_LIST 5      //!< query a client list =>response by a SIGNAL
#define MSG_QUERY_TERRAIN 6 //!< query terrain name
#define MSG_TERRAIN 7       //!< tell what terrain to load, or sets what terrain to load
#define MSG_VERSION 8       //!< tells what network version is running
#define MSG_VEHICLE 9       //!< P2P tells which vehicle to load
#define MSG_VDATA 10        //!< P2P vehicle data (large)

#define MSG2_HELLO 1000     //!< client sends its version as first message
#define MSG2_VERSION 1001   //!< server responds with its version

#define MSG2_FULL 1002      //!< no more place
#define MSG2_BANNED 1003    //!< Uh
#define MSG2_WELCOME 1004   //!< we can proceed

#define MSG2_USE_VEHICLE 1005 //!< the client says which vehicle it uses
#define MSG2_SPAWN 1006       //!< the server asks to spawn a new vehicle

#define MSG2_BUFFER_SIZE 1007  //!< the clients tells the buffer size to use for
                               //!< the selected vehicle
#define MSG2_VEHICLE_DATA 1008 //!< data block

#define MSG2_USER 1009   //!< user credentials
#define MSG2_DELETE 1010 //!< delete a disappearing truck

#define MSG2_CHAT 1011  //!< chat line
#define MSG2_FORCE 1012 //!< Force information, attached to a netforce_t struct

//#define MSG2_LISTUSERS 1013      //!< asks for connected users
//#define MSG2_LISTUSERS_RESP 1014 //!< replies with connected users

//#define MSG2_PASSWORD 1015      //!< requests password
//#define MSG2_PASSWORD_RESP 1016 //!< replies password

// 2.1
#define MSG2_USER_CREDENTIALS 1017 //!< improved user credentials
//#define MSG2_TERRAIN 1018          //!< client asks server for the terrain name
#define MSG2_TERRAIN_RESP 1019     //!< server send client the terrain name
#define MSG2_WRONG_PW 1020         //!< server send that on wrong pw

#define MSG2_RCON_LOGIN 1021         //!< client send that the server with a
                                     //!< sha1 password
#define MSG2_RCON_LOGIN_FAILED 1022  //!< server sends that on wrong pw
#define MSG2_RCON_LOGIN_SUCCESS 1023 //!< server sends that on correct pw
#define MSG2_RCON_LOGIN_NOTAV 1024   //!< server sends that on disabled rcon

#define MSG2_RCON_COMMAND 1025         //!< sends a rcon command
#define MSG2_RCON_COMMAND_FAILED 1026  //!< send to client
#define MSG2_RCON_COMMAND_SUCCESS 1027 //!< send to client

// 2.1 but only active in 0.35
#define MSG2_GAME_CMD 1028  //!< send to client from server only
typedef struct
{
	char username[20];
	char password[40];
	char uniqueid[40];
} user_credentials_t;

typedef struct
{
	unsigned int command;
	int source;
	unsigned int size;
} header_t;

typedef struct
{
	unsigned int target_uid;
	unsigned int node_id;
	float fx;
	float fy;
	float fz;
} netforce_t;

#define NETMASK_HORN        0x00000001
#define NETMASK_LIGHTS      0x00000002
#define NETMASK_BRAKES      0x00000004
#define NETMASK_REVERSE     0x00000008
#define NETMASK_BEACONS     0x00000010
#define NETMASK_BLINK_LEFT  0x00000020
#define NETMASK_BLINK_RIGHT 0x00000040
#define NETMASK_BLINK_WARN  0x00000080
#define NETMASK_CLIGHT1     0x00000100
#define NETMASK_CLIGHT2     0x00000200
#define NETMASK_CLIGHT3     0x00000400
#define NETMASK_CLIGHT4     0x00000800
#define NETMASK_POLICEAUDIO 0x00001000
#define NETMASK_PARTICLE    0x00002000

typedef struct
{
	int time;
	float engine_speed;
	float engine_force;
	unsigned int flagmask;
} oob_t;


//REGISTRY STUFF
#define REPO_SERVER "api.rigsofrods.com"
#define REPO_URLPREFIX ""

//used by configurator
#define REPO_HTML_SERVERLIST "http://api.rigsofrods.com/serverlist/"

//debugging
//#define REFLECT_DEBUG 

// strnlen is nto a std function, this macro can be used in place.
#ifdef NO_STRNLEN
#define strnlen(a, b) strlen(a)
#endif
#endif
