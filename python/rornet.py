import struct

RORNET_VERSION = "RoRnet_2.3"

MSG2_HELLO         = 1000  # client sends its version as first message
MSG2_VERSION       = 1001  # server responds with its version

MSG2_FULL          = 1002  # no more place
MSG2_BANNED        = 1003  # Uh
MSG2_WELCOME       = 1004  # we can proceed

MSG2_USE_VEHICLE   = 1005  # the client says which vehicle it uses
MSG2_USE_VEHICLE2  = 1040  # the client says which vehicle it uses, 2nd version

MSG2_BUFFER_SIZE   = 1007  # the clients tells the buffer size to use for the selected vehicle
MSG2_VEHICLE_DATA  = 1008  # data block

MSG2_USER          = 1009  # user credentials
MSG2_DELETE        = 1010  # delete a disappearing truck

MSG2_CHAT          = 1011  # chat line
MSG2_FORCE         = 1012  # Force information, attached to a netforce_t struct


# 2.1
MSG2_USER_CREDENTIALS = 1017 # improved user credentials
MSG2_TERRAIN_RESP     = 1019 # server send client the terrain name
MSG2_WRONG_PW         = 1020 # server send that on wrong pw

MSG2_RCON_LOGIN       = 1021 # client send that the server with a sha1 password
MSG2_RCON_LOGIN_FAILED = 1022  # server sends that on wrong pw
MSG2_RCON_LOGIN_SUCCESS = 1023 # server sends that on correct pw
MSG2_RCON_LOGIN_NOTAV = 1024   # server sends that on disabled rcon

MSG2_RCON_COMMAND         = 1025 # sends a rcon command
MSG2_RCON_COMMAND_FAILED  = 1026 # send to client
MSG2_RCON_COMMAND_SUCCESS = 1027 # send to client

# 2.1 but only active in 0.35 and up
MSG2_GAME_CMD             = 1028 # send to client from server only

# 2.1 but only active in 0.36 and up
MSG2_USER_CREDENTIALS2    = 1018 # improved user credentials
MSG2_USER_INFO            = 1029 # improved user data that is sent from the server to the clients
MSG2_PRIVCHAT             = 1038 # sent from client to server to send private chat messages

# new stream functions:
MSG2_STREAM_REGISTER      = 1030 # create new stream
MSG2_STREAM_REGISTER_RESP = 1031 # reply from server to registering client
MSG2_STREAM_CONTROL_FLOW = 1032 # suspend/unsuspend streams
MSG2_STREAM_CONTROL_FLOW_RESP = 1033  # reply from server to requesting client
MSG2_STREAM_UNREGISTER = 1034         # remove stream
MSG2_STREAM_UNREGISTER_RESP = 1035    # remove stream response from server to requsting client
MSG2_STREAM_TAKEOVER = 1036           # stream takeover
MSG2_STREAM_TAKEOVER_RESP = 1037      # stream takeover response from server
MSG2_STREAM_DATA = 1039               # stream data
MSG2_USER_JOIN = 1049
MSG2_USER_LEAVE = 1050

# helper function to return the variable name
def commandName(cmd):
  vars = globals()
  for c in vars:
    if vars[c] == cmd and len(c) > 4 and c[0:5] == "MSG2_":
	  return c[5:]

# utility functions
def registerStreamData(streamid, name, type, status):
	name = "% 128s" % name
	return struct.pack('i128sii', streamid, name, type, status)

def processStreamRegisterData(data):
	streamid, name, type, status = struct.unpack('i128sii', data)
	print "# Stream registration: "
	print "# * ID: %d" % int(streamid)
	print "# * name: '%s'" % name.strip()
	print "# * type: %d" % int(type)
	print "# * status: %d" % int(status)

def processUserInfo(data):
	truckname, username, language, clientinfo, flagmask = struct.unpack('128s20s5s20sI', data)
	print "# User Information: "
	print "# * truckname: '%s'" % truckname.strip()
	print "# * username: '%s'" % username.strip()
	print "# * language: '%s'" % language.strip()
	print "# * clientinfo: '%s'" % clientinfo.strip()
	print "# * flagmask: %d" % int(flagmask)

def processUserOnJoinInfo(data):
	version, nickname, authstatus, slotnum = struct.unpack('c20sII', data)
	print "# User OnJoin Information: "
	print "# * version: %s" % (version)
	print "# * nickname: '%s'" % nickname.strip()
	print "# * authstatus: %s" % (authstatus)
	print "# * slotnum: %s" % (slotnum)
