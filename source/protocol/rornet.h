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
#ifndef RORNETPROTOCOL_H__
#define RORNETPROTOCOL_H__

#define BITMASK(x) (1 << (x-1)) 

// protocol settings
static const int   MAX_PEERS = 64;
static const int   MAX_MESSAGE_LENGTH = 32768; // higher value, since we also send around the beam data!

// protocol version
static const char *RORNET_VERSION = "RoRnet_2.33";

// REGISTRY STUFF
static const char *REPO_SERVER = "api.rigsofrods.com";
static const char *REPO_URLPREFIX = "";

// used by configurator
static const char *REPO_HTML_SERVERLIST = "http://api.rigsofrods.com/serverlist/";
static const char *NEWS_HTML_PAGE = "http://api.rigsofrods.com/news/";

// ENUMs
enum {
	MSG2_HELLO  = 1000,              //!< client sends its version as first message
	// hello responses
	MSG2_FULL,                       //!< no more slots for us
	MSG2_WRONG_PW,                   //!< server send that on wrong pw
	MSG2_WRONG_VER,                  //!< wrong version
    MSG2_BANNED,                     //!< Uh
	MSG2_WELCOME,                    //!< we can proceed
	
	MSG2_VERSION,                    //!< server responds with its version
	MSG2_USE_VEHICLE,                //!< the client says which vehicle it uses
	
	MSG2_REQUEST_VEHICLE_BEAMS,      //!< sent by the client to request beam data
	MSG2_VEHICLE_BEAMS,              //!< contains beam information

	MSG2_CHAT,                       //!< chat line
	MSG2_FORCE,                      //!< Force information, attached to a netforce_t struct
	MSG2_LISTUSERS,                  //!< asks for connected users

	MSG2_USER_CREDENTIALS,           //!< improved user credentials
	MSG2_TERRAIN_RESP,               //!< server send client the terrain name

	MSG2_GAME_CMD,                   //!< send to client from server only

	MSG2_USER_CREDENTIALS2,          //!< improved user credentials
	MSG2_USER_INFO,                  //!< improved user data that is sent from the server to the clients
	MSG2_PRIVCHAT,                   //!< sent from client to server to send private chat messages

	// stream functions
	MSG2_STREAM_REGISTER,            //!< create new stream
	MSG2_STREAM_REGISTER_RESP,       //!< reply from server to registering client
	MSG2_STREAM_CONTROL_FLOW,        //!< suspend/unsuspend streams
	MSG2_STREAM_CONTROL_FLOW_RESP,   //!< reply from server to requesting client
	MSG2_STREAM_UNREGISTER,          //!< remove stream
	MSG2_STREAM_UNREGISTER_RESP,     //!< remove stream response from server to requsting client
	MSG2_STREAM_TAKEOVER,            //!< stream takeover
	MSG2_STREAM_TAKEOVER_RESP,       //!< stream takeover response from server
	MSG2_STREAM_DATA,                //!< stream data
	MSG2_USER_JOIN,                  //!< new user joined
	MSG2_USER_LEAVE,                 //!< user leaves

	// master server interaction
	MSG2_MASTERINFO,                 //!< master information response
};

enum {
	AUTH_NONE   = 0,
	AUTH_ADMIN  = BITMASK(1),
	AUTH_RANKED = BITMASK(2),
	AUTH_MOD    = BITMASK(3),
	AUTH_BOT    = BITMASK(4),
};


enum {
	NETMASK_HORN        = BITMASK(1),
	NETMASK_LIGHTS      = BITMASK(2),
	NETMASK_BRAKES      = BITMASK(3),
	NETMASK_REVERSE     = BITMASK(4),
	NETMASK_BEACONS     = BITMASK(5),
	NETMASK_BLINK_LEFT  = BITMASK(6),
	NETMASK_BLINK_RIGHT = BITMASK(7),
	NETMASK_BLINK_WARN  = BITMASK(8),
	NETMASK_CLIGHT1     = BITMASK(9),
	NETMASK_CLIGHT2     = BITMASK(10),
	NETMASK_CLIGHT3     = BITMASK(11),
	NETMASK_CLIGHT4     = BITMASK(12),
	NETMASK_POLICEAUDIO = BITMASK(13),
	NETMASK_PARTICLE    = BITMASK(14),
};

// structs
typedef struct
{
	char version;
	char nickname[20];
	int authstatus;
	int slotnum;
	int colournum;
} client_info_on_join;

typedef struct
{
	short int node1;
	short int node2;
	char type;
} simple_beam_info;

typedef struct
{
	char version;
	unsigned int nodebuffersize;
	unsigned int netbuffersize;
	unsigned int free_wheel;
	unsigned int nodecount;
	unsigned int beamcount;
} simple_beam_info_header;

// structure to control flow of a stream, send in both directions
typedef struct
{
	int sid;                  //!< the unique id of the stream
	int old_uid;              //!< old owner
	int new_uid;              //!< new owner
} stream_takeover_t;

// structure that is send from the cleint to server and vice versa, to broadcast a new stream
typedef struct
{
	char name[128];           //!< the truck filename
	int type;                 //!< stream type
	int status;               //!< initial stream status
	char data[8000];		  //!< data used for stream setup
} stream_register_t;

// structure to control flow of a stream, send in both directions
typedef struct
{
	int sid;                  //!< the unique id of the stream
	int status;               //!< the rquested / proposed status
} stream_control_t;

// structure sent to remove a stream
typedef struct
{
	int sid;                  //!< the unique id of the stream
} stream_unregister_t;

// structure sent from server to clients to update their user information
typedef struct
{
	char truckname[128];      //!< the truck filename
	char username[20];        //!< the nickname of the user
	char language[5];         //!< user's language. For example "de-DE" or "en-US"
	char clientinfo[20];      //!< client info, like 'RoR-0.35'
	unsigned int flagmask;    //!< flags. Like moderator/admin, authed/non-authed, etc
} user_info_t;

typedef struct
{
	char username[20];
	char password[40];
	char uniqueid[40];
} user_credentials_t;

typedef struct
{
	char username[20];         //!< the nickname of the user
	char password[40];         //!< server password
	char token[40];            //!< user token
	char language[5];          //!< user's language. For example "de-DE" or "en-US"
	char clientname[10];       //!< the name and version of the client. For exmaple: "ror" or "gamebot"
	int  clientversion;        //!< a version number of the client. For example 1 for RoR 0.35
	char sessiontype[10];      //!< the requested session type. For example "normal", "bot", "rcon"
	char sessionoptions[128];  //!< reserved for future options
} user_credentials2_t;

typedef struct
{
	unsigned int command;
	int source;
	unsigned int streamid;
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

typedef struct
{
	int time;
	float engine_speed;
	float engine_force;
	unsigned int flagmask;
} oob_t;

#endif //RORNETPROTOCOL_H__
