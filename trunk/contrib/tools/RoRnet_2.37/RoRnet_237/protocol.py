import struct, time
from utils import vector3, vector4

RORNET_VERSION = "RoRnet_2.37"

MSG2_HELLO                      = 1000                #!< client sends its version as first message
# hello responses
MSG2_FULL                       = 1001                #!< no more slots for us
MSG2_WRONG_PW                   = 1002                #!< server send that on wrong pw
MSG2_WRONG_VER                  = 1003                #!< wrong version
MSG2_BANNED                     = 1004                #!< client not allowed to join
MSG2_WELCOME                    = 1005                #!< we can proceed

MSG2_VERSION                    = 1006                #!< server responds with its version

MSG2_CHAT_OBSOLETE              = 1007                #!< chat line (deprecated, use the UTF VERSIONS BELOW!)

MSG2_SERVER_SETTINGS            = 1008                #!< server send client the terrain name: server_info_t

MSG2_GAME_CMD                   = 1009                #!< Script message. Can be sent in both directions.

MSG2_USER_INFO                  = 1010                #!< user data that is sent from the server to the clients
MSG2_PRIVCHAT_OBSOLETE          = 1011                #!< sent from client to server to send private chat messages
                                                      #!<  (deprecated, use the UTF VERSIONS BELOW!)

# stream functions
MSG2_STREAM_REGISTER            = 1012                #!< create new stream
MSG2_STREAM_REGISTER_RESULT     = 1013                #!< result of a stream creation
#MSG2_STREAM_REGISTER_RESP      				      #!< reply from server to registering client
#MSG2_STREAM_CONTROL_FLOW          					  #!< suspend/unsuspend streams
#MSG2_STREAM_CONTROL_FLOW_RESP     			 		  #!< reply from server to requesting client
#MSG2_STREAM_UNREGISTER            					  #!< remove stream
#MSG2_STREAM_UNREGISTER_RESP       					  #!< remove stream response from server to requsting client
#MSG2_STREAM_TAKEOVER              					  #!< stream takeover
#MSG2_STREAM_TAKEOVER_RESP        					  #!< stream takeover response from server
MSG2_STREAM_DATA                = 1014                #!< stream data
MSG2_USER_JOIN                  = 1015                #!< new user joined
MSG2_USER_LEAVE                 = 1016                #!< user leaves

MSG2_NETQUALITY                 = 1017                #!< network quality information

# master server interaction
MSG2_MASTERINFO                 = 1018                #!< master information response

MSG2_UTF_CHAT                   = 1019                #!< chat line in UTF encoding
MSG2_UTF_PRIVCHAT               = 1020                #!< private chat line in UTF encoding

#Character commands
CHARCMD_POSITION = 0
CHARCMD_ATTACH   = 1

#Character modes
CHAR_IDLE_SWAY = "Idle_sway"
CHAR_SPOT_SWIM = "Spot_swim"
CHAR_WALK      = "Walk"
CHAR_RUN       = "Run"
CHAR_SWIM_LOOP = "Swim_loop"
CHAR_TURN      = "Turn"
CHAR_DRIVING   = "driving"

# authoirizations
AUTH_NONE   = 0          #!< no authentication
AUTH_ADMIN  = 1          #!< admin on the server
AUTH_RANKED = 2          #!< ranked status
AUTH_MOD    = 4          #!< moderator status
AUTH_BOT    = 8          #!< bot status
AUTH_BANNED = 16         #!< banned

# TYPES
TYPE_TRUCK     = 0
TYPE_CHARACTER = 1
TYPE_AI        = 2
TYPE_CHAT      = 3

# NETMASKS
NETMASK_HORN        = 1    #!< horn is in use
NETMASK_LIGHTS      = 2    #!< lights on
NETMASK_BRAKES      = 4    #!< brake lights on
NETMASK_REVERSE     = 8    #!< reverse light on
NETMASK_BEACONS     = 16   #!< beacons on
NETMASK_BLINK_LEFT  = 32   #!< left blinker on
NETMASK_BLINK_RIGHT = 64   #!< right blinker on
NETMASK_BLINK_WARN  = 128  #!< warn blinker on
NETMASK_CLIGHT1     = 256  #!< custom light 1 on
NETMASK_CLIGHT2     = 512  #!< custom light 2 on
NETMASK_CLIGHT3     = 1024 #!< custom light 3 on
NETMASK_CLIGHT4     = 2048 #!< custom light 4 on
NETMASK_POLICEAUDIO = 4096 #!< police siren on
NETMASK_PARTICLE    = 8192 #!< custom particles on


#
# PROCESSING OF INCOMING MESSAGES
#

def processCharacterAttachData(data):
	s = charAttach_data_t()
	s.command, s.enabled, s.source_id, s.stream_id, s.position = struct.unpack('i?3i', data)
	return s

def processCharacterPosData(data):
	s = charPos_data_t()
	if RORNET_VERSION == "RoRnet_2.34":
		s.command, s.pos.x, s.pos.z, s.pos.y, s.rot.x, s.rot.y, s.rot.z, s.rot.w, s.animationMode, s.animationTime = struct.unpack('i7f28sf', data)
	else: # RoRnet_2.35 and later
		s.command, s.pos.x, s.pos.z, s.pos.y, s.rot.x, s.rot.y, s.rot.z, s.rot.w, s.animationMode, s.animationTime = struct.unpack('i7f255sf', data)
	s.animationMode = s.animationMode.strip('\0')
	return s

def processCharacterData(data):
	thecommand = struct.unpack('i', data[0:4])[0]
	if thecommand == CHARCMD_POSITION:
		return processCharacterPosData(data)
	if thecommand == CHARCMD_ATTACH:
		return processCharacterAttachData(data)
	else:
		return charPos_data_t()
				

def processTruckData(data):
	s = truckStream_data_t()
	s.time, s.engine_speed, s.engine_force, s.flagmask, s.refpos.x, s.refpos.z, s.refpos.y, s.node_data = struct.unpack('i2fI3f%ds' % (len(data)-28), data)
	return s
	
def processRegisterStreamData(data):
	s = stream_info_t()
	type = struct.unpack('i', data[128:132])[0]
	if type == TYPE_CHAT or type == TYPE_CHARACTER:
		s.name, s.type, s.status, s.origin_sourceid, s.origin_streamid, s.regdata = struct.unpack('128s4i8000s', data)
	elif type == TYPE_TRUCK:
		if len(data) == 8144:
			s.name, s.type, s.status, s.origin_sourceid, s.origin_streamid, s.regdata = struct.unpack('128s4i8000s', data)
		elif len(data)==748:
			s.name, s.type, s.status, s.origin_sourceid, s.origin_streamid, s.bufferSize, s.regdata = struct.unpack('128s5i600s', data)
	s.name = s.name.strip('\0')
	return s

def processUserInfo(data):
	u = user_info_t()
	u.uniqueID, u.username, u.usertoken, u.serverpassword, u.language, u.clientname, u.clientversion, u.clientGUID, u.sessiontype, u.sessionoptions, u.authstatus, u.slotnum, u.colournum = struct.unpack('I40s40s40s10s10s25s40s10s128s3i', data)
	u.username       = u.username.strip('\0')
	u.usertoken      = u.usertoken.strip('\0')
	u.serverpassword = u.serverpassword.strip('\0')
	u.language       = u.language.strip('\0')
	u.clientname     = u.clientname.strip('\0')
	u.clientversion  = u.clientversion.strip('\0')
	u.clientGUID     = u.clientGUID.strip('\0')
	u.sessiontype    = u.sessiontype.strip('\0')
	u.sessionoptions = u.sessionoptions.strip('\0')
	return u
	
def processServerInfo(data):
	s = server_info_t()
	s.protocolversion, s.terrain, s.servername, s.passworded, s.info = struct.unpack('20s128s128s?4096s', data)
	s.protocolversion = s.protocolversion.strip('\0')
	s.terrain         = s.terrain.strip('\0')
	s.servername      = s.servername.strip('\0').replace('%20', ' ')
	s.info            = s.info.strip('\0')
	return s
	

def processNetQuality(data):
	(quality) = struct.unpack('I', data)
	return quality
	
#
# GENERAL UTILITIES
#
# helper function to return the variable name
def commandName(cmd):
  vars = globals()
  for c in vars:
    if vars[c] == cmd and len(c) > 4 and ( c[0:5] == "MSG2_"):
	  return c[5:]

#
# STRUCTS
#
class user_info_t:
	def __init__(self):
		self.uniqueID       = 0
		self.username       = ""
		self.usertoken      = ""
		self.serverpassword = ""
		self.language       = ""
		self.clientname     = ""
		self.clientversion  = ""
		self.sessiontype    = ""
		self.sessionoptions = ""
		self.authstatus     = 0
		self.slotnum        = -1
		self.colournum      = -1
		self.clientGUID     = ""
	
	def update(self, u):
		t = user_info_t()
		if u.uniqueID != t.uniqueID:
			self.uniqueID = u.uniqueID
			
		if u.username != t.username:
			self.username = u.username
			
		if u.language != t.language:
			self.language = u.language
			
		if u.clientname != t.clientname:
			self.clientname = u.clientname
			
		if u.clientversion != t.clientversion:
			self.clientversion = u.clientversion
			
		if u.sessiontype != t.sessiontype:
			self.sessiontype = u.sessiontype
			
		if u.sessionoptions != t.sessionoptions:
			self.sessionoptions = u.sessionoptions
			
		if u.authstatus != t.authstatus:
			self.authstatus = u.authstatus
			
		if u.slotnum != t.slotnum:
			self.slotnum = u.slotnum
			
		if u.colournum != t.colournum:
			self.colournum = u.colournum
		del t

class stream_info_t:
	def __init__(self):
		self.name = ""
		self.fileExt = ""
		self.type = -1
		self.status = -1
		self.origin_sourceid = -1
		self.origin_streamid = -1
		self.bufferSize = -1
		self.regdata = ""
		self.refpos = vector3()
		self.rot = vector4()

class truckStream_data_t:
	def __init__(self):
		self.time = -1
		self.engine_speed = 0.0
		self.engine_force = 0.0
		self.flagmask = 0
		self.refpos = vector3()
		self.node_data = ""

class charPos_data_t:
	def __init__(self):
		self.command       = -1
		self.pos           = vector3()
		self.rot           = vector4()
		self.animationMode = ""
		self.animationTime = 0.0

class charAttach_data_t:
	def __init__(self):
		self.command   = -1
		self.enabled   = False
		self.source_id = -1
		self.stream_id = -1
		self.position  = -1

class server_info_t:
	def __init__(self):
		self.host            = ""
		self.port            = 12000
		self.protocolversion = RORNET_VERSION
		self.terrain         = ""
		self.servername      = ""
		self.passworded      = False
		self.password        = ""
		self.info            = ""

	def update(self, u):
		t = server_info_t()
		if u.terrain != t.terrain:
			self.terrain = u.terrain
		if u.servername != t.servername:
			self.servername = u.servername
		if u.info != t.info:
			self.info = u.info
		del t

class DataPacket:
	source=0
	command=0
	streamid=0
	size=0
	data=0
	time=0
	def __init__(self, command, source, streamid, size, data):
		self.source = source
		self.command = command
		self.streamid = streamid
		self.size = size
		self.data = data
		self.time = time.time()