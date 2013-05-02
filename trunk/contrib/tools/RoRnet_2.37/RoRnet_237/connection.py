####################################
#  RIGS OF RODS CONNECTION LIBRARY #
####################################

import struct, threading, socket, time, string, math, logging, Queue, hashlib, random

from streamManager import streamManager
from utils import *
from protocol import *

def errorHandler(msg):
	print msg

# This class does all communication with the RoR server.
# This is the class you'll need to make your own bot (together with DataPacket, the streamManager and the RoRnet file)
class RoR_Connection:
	def __init__(self, ignoreStreamData = False, messager = errorHandler):
	
		# Initialize variables
		self.socket = None
		self.runCondition = 1
		self.streamID = 10 # streamnumbers under 10 are reserved for other stuff
		self.headersize = struct.calcsize('IIII')
		self.uid = 0
		self.receivedMessages = Queue.Queue()
		self.netQuality = 0
		self.connectTime = 0
		self.dropIncStreamData = ignoreStreamData
		
		# Initialize logger
		self.log = messager
		
		# Initialize the stream manager
		self.sm = streamManager()


	def isConnected(self):
		return (self.socket != None)


	# Knock, knock - Who's there? - The Master Server! - Really? - No, but we pretend to be one :)
	# Useful to check if a server is online and to get the protocol version of the server.
	def knockServer(self, host, port):
		try:
			sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		except socket.error, msg:
			sock = None
			self.log("Couldn't create socket.")
			return None
			
		sock.settimeout(2)

		try:
			sock.connect((u"%s" % host, port))
		except socket.error, msg:
			sock.close()
			sock = None
			self.log("Couldn't connect to server %s:%d" % (host, port))
			return None
		
		if sock is None:
			return None

		# send hello
		data = "MasterServer"
		try:
			sock.send(struct.pack('IIII'+str(len(data))+'s', MSG2_HELLO, 5000, 0, len(data), str(data)))
		except Exception, e:
			self.log('sendMsg error: '+str(e))
			return None
		

		# receive answer
		data = ""
		tmp = ""
		errorCount = 0
		try:
			while len(data)<self.headersize:
				try:
					tmp = sock.recv(self.headersize-len(data))
				except socket.timeout:
					continue
				
				# unfortunately, we have to do some stupid stuff here, to avoid infinite loops...
				if not tmp:
					errorCount += 1;
					if errorCount > 3:
						# lost connection
						self.log("Connection error #ERROR_CON005")
						break
					continue
				else:
					data += tmp
		
			if not data or errorCount > 3:
				# lost connection
				self.log("Connection error #ERROR_CON008")
				sock.close()
				sock = None
				return None
		
			(command, source, streamid, size) = struct.unpack('IIII', data)

			data = ""
			tmp = ""
			while len(data)<size:
				try:
					tmp = sock.recv(size-len(data))
				except socket.timeout:
					continue
				
				# unfortunately, we have to do some stupid stuff here, to avoid infinite loops...
				if not tmp:
					errorCount += 1;
					if errorCount > 3:
						# lost connection
						self.log("Connection error #ERROR_CON006")
						break
					continue
				else:
					data += tmp
		except socket.error:
			self.log("Connection error #ERROR_CON015")
			sock.close()
			sock = None
			return None

		if not data or errorCount > 3:
			# lost connection
			self.log("Connection error #ERROR_CON007")
			sock.close()
			sock = None
			return None;
	
		content = struct.unpack(str(size) + 's', data)[0]

		return DataPacket(command, source, streamid, size, content)

	def connect(self, user, serverinfo):
		# empty queue
		while 1:
			try:
				self.receivedMessages.get_nowait()
			except Queue.Empty:
				break
				
		if len(user.language)==0:
			user.language = "en_GB"
		# TODO: check the rest of the input
		
		# reset some variables
		self.streamID = 10
		self.uid = 0
		self.netQuality = 0
		self.serverinfo = serverinfo

		self.log("Creating socket...")
		self.runCondition = 1
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		try:
			self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		except socket.error, msg:
			self.socket = None
			self.log("Couldn't create socket.")
			return False
						
		try:
			self.socket.connect((u"%s" % self.serverinfo.host, self.serverinfo.port))
		except socket.error, msg:
			self.socket.close()
			self.socket = None
			self.log("Couldn't connect to server %s:%d" % (self.serverinfo.host, self.serverinfo.port))
			return False

		self.receiveThread = threading.Thread(target=self._start_receive_thread)
		self.receiveThread.setDaemon(True)
		self.receiveThread.start()

		# send hello
		self.log("Successfully connected! Sending hello message.")
		self._sendHello(self.serverinfo.protocolversion)

		# receive hello
		packet = self.receiveMsg()

		if packet is None:
			self.log("Server didn't respond.")
			return False

		if packet.command != MSG2_HELLO:
			self.log("Received %s while expecting MSG2_HELLO Exiting..." % (commandName(packet.command)))
			self.disconnect()
			return False
		self.log("Received server info.")
		self.serverinfo.update(processServerInfo(packet.data))

		self.log("Sending our user info.")
		self._sendUserInfo(user)

		# receive a welcome message with our own user data
		packet = self.receiveMsg()
		
		if packet is None:
			self.log("Server sent nothing, while it should have sent us a welcome message.")
			return False
		
		# Some error handling
		if packet.command != MSG2_WELCOME:
			if packet.command == MSG2_FULL:
				self.log("This server is full :(")
			elif packet.command == MSG2_BANNED:
				self.log("We're banned from this server :/")
			elif packet.command == MSG2_WRONG_PW:
				self.log("Wrong password :|")
			elif packet.command == MSG2_WRONG_VER:
				self.log("Wrong protocol version! O.o")
			else:
				self.log("invalid handshake: MSG2_HELLO (server error?)")
			self.log("Unable to connect to this server. Exiting...")
			self.disconnect()
			return False
		
		# process our userdata
		user.update(processUserInfo(packet.data))
		self.log("joined as '%s' on slot %d with UID %s and auth %d" % (user.username, user.slotnum, str(user.uniqueID), user.authstatus))
		self.sm.updateClient(user)
		self.uid = user.uniqueID
		
		# Receive the user join message
		# (it's actually not really necessary to wait for this message)
		# packet = self.receiveMsg()
		# if packet.command != MSG2_USER_JOIN:
			# self.log("Missing message USER_JOIN. Connection failed.")
			# self.disconnect()
			# return False
			
		# register character stream
		s = stream_info_t()
		s.name = "default"
		s.type = TYPE_CHARACTER
		s.status = 0
		s.regdata = chr(2)
		self.registerStream(s)
		
		# register chat stream
		s = stream_info_t()
		s.name = "chat"
		s.type = TYPE_CHAT
		s.status = 0
		s.regdata = 0
		self.registerStream(s)
		
		# set the time when we connected (needed to send stream data)
		self.connectTime = time.time()
		
		# Let the server initialize us.
		# The only way to tell the server that we're ready, is by streaming a frame (TODO: improve server with MSG2_INITIALIZED)
		self.streamCharacter(
			vector3(0,0,0),   # (posx, posy, posz)
			vector4(0,0,0,0), # (rotx, roty, rotz, rotw)
			"Idle_sway",      # animationMode[255]
			random.random()   # animationTime
		)
		
		# successfully connected
		return True

	# Disconnects from the server
	def disconnect(self):
		self.log("Disconnecting...")
		self.runCondition = 0
		self.receiveThread.join()
		if not self.socket is None:
			self._sendUserLeave()
			self.log('closing socket')
			self.socket.close()
		self.socket = None
	
	# Internal use only!
	def _sendUserInfo(self, user):
		data = struct.pack('I40s40s40s10s10s25s40s10s128siii', 
			int(user.uniqueID),
			user.username,
			string.upper(hashlib.sha1(user.usertoken).hexdigest()),
			string.upper(hashlib.sha1(user.serverpassword).hexdigest()),
			str(user.language),
			str(user.clientname),
			str(user.clientversion),
			str(user.clientGUID),
			str(user.sessiontype),
			str(user.sessionoptions),
			int(user.authstatus),
			int(user.slotnum),
			int(user.colournum)
		)		
		self.sendMsg(DataPacket(MSG2_USER_INFO, 0, 0, len(data), data))
	
	# Internal use only!
	def _sendHello(self, version):
		self.sendMsg(DataPacket(MSG2_HELLO, 0, 0, len(version), version))
	
	# Internal use only!
	# use disconnect() instead
	def _sendUserLeave(self):
		self.sendMsg(DataPacket(MSG2_USER_LEAVE, self.uid, 0, 0, 0))
	
	# Register a new stream
	def registerStream(self, s):
		s.origin_sourceid = self.uid
		s.origin_streamid = self.streamID
		if type==TYPE_TRUCK:
			data = struct.pack('128siiiii600s', s.name, s.type, s.status, s.origin_sourceid, s.origin_streamid, s.bufferSize, str(s.regdata))
		else:
			data = struct.pack('128siiii8000s', s.name, s.type, s.status, s.origin_sourceid, s.origin_streamid, str(s.regdata))
		self.sendMsg(DataPacket(MSG2_STREAM_REGISTER, s.origin_sourceid, s.origin_streamid, len(data), data))
		self.sm.addStream(s)
		self.streamID += 1	
		return s.origin_streamid

	# Unregister a stream
	# NOTE: This feature is not supported by the offcial RoR server or clients
	def unregisterStream(self, streamID):
		data = struct.pack('i', streamID)
		# MSG2_STREAM_UNREGISTER is not supported by the current RoRnet protocol
		# self.sendMsg(DataPacket(MSG2_STREAM_UNREGISTER, self.uid, self.streamID, len(data), data))
		self.sm.delStream(self.uid, streamID)

	# Replies to a stream register message
	def replyToStreamRegister(self, data, status):
		# TODO: Is this correct, according to the RoRnet_2.3 specifications?
		data_out = struct.pack('128siiii8000s', data.name, data.type, status, data.origin_sourceid, data.origin_streamid, data.regdata)
		self.sendMsg(DataPacket(MSG2_STREAM_REGISTER_RESULT, self.uid, data.origin_streamid, len(data_out), data_out))
	
	# Stream character data
	def streamCharacter(self, pos, rot, animMode, animTime):
		# pack: command, posx, posy, posz, rotx, roty, rotz, rotw, animationMode[255], animationTime
		if RORNET_VERSION == "RoRnet_2.34":
			data = struct.pack('i7f28sf', CHARCMD_POSITION, pos.x, pos.z, pos.y, rot.x, rot.y, rot.z, rot.w, animMode, animTime)
		else:
			data = struct.pack('i7f255sf', CHARCMD_POSITION, pos.x, pos.z, pos.y, rot.x, rot.y, rot.z, rot.w, animMode, animTime)
		self.sendMsg(DataPacket(MSG2_STREAM_DATA, self.uid, self.sm.getCharSID(self.uid), len(data), data))
	
	# Stream truck data
	def streamTruck(self, s, streamID, recalcTime = True):
		if recalcTime:
			theTime = math.floor((time.time()-self.connectTime)*1000)
		else:
			theTime = s.time
		data = struct.pack('i2fI3f%ds' % len(s.node_data), theTime, s.engine_speed, s.engine_force, s.flagmask, s.refpos.x, s.refpos.z, s.refpos.y, s.node_data)
		self.sendMsg(DataPacket(MSG2_STREAM_DATA, self.uid, streamID, len(data), data))

	# Attach the character to a truck
	# TODO: implement this
	def attachCharacter(self, enabled, position):
		self.log("Call to a not-implemented function: attachCharacter.")
	
	# Sends a chat message
	def sendChat(self, msg):
		if not self.socket:
			return False
		
		self.sendMsg(DataPacket(MSG2_UTF_CHAT, self.uid, self.sm.getChatSID(self.uid), len(msg), unicode(msg, errors='ignore').encode('utf-8')))
		self.log("msg sent: %s" % (msg))
		
		return True
	
	# Send chat and split the message into mutiple parts if required
	def sendChat_splitted(self, msg):
		if not self.socket:
			return False
		
		# word wrap size	
		maxsize = 100
		if len(msg) > maxsize:
			self.log("%d=len(msg)>maxsize=%d" % (len(msg), maxsize))
			for i in range(0, int(math.ceil(float(len(msg)) / float(maxsize)))):
				if i == 0:
					msga = msg[maxsize*i:maxsize*(i+1)]
					self.log("sending %s" % (msga))
					self.sendMsg(DataPacket(MSG2_UTF_CHAT, self.uid, self.sm.getChatSID(self.uid), len(msga), unicode(msga, errors='ignore').encode('utf-8')))
				else:
					msga = "| "+msg[maxsize*i:maxsize*(i+1)]
					self.log("sending %s" % (msga))
					self.sendMsg(DataPacket(MSG2_UTF_CHAT, self.uid, self.sm.getChatSID(self.uid), len(msga), unicode(msga, errors='ignore').encode('utf-8')))
		else:
			self.sendMsg(DataPacket(MSG2_UTF_CHAT, self.uid, self.sm.getChatSID(self.uid), len(msg), unicode(msg, errors='ignore').encode('utf-8')))
		self.log("msg sent: %s" % (msg))
		
		return True
	
	# Sends a private chat message
	def privChat(self, uid, msg):
		if not self.socket:
			return False

		self.log("sending PRIVCHAT message")
		data = struct.pack("I8000s", uid, unicode(msg, errors='ignore').encode('utf-8'))
		self.sendMsg(DataPacket(MSG2_UTF_PRIVCHAT, self.uid, self.sm.getChatSID(self.uid), len(data), data))
		
		return True
	
	# Send a script command
	def sendGameCmd(self, msg):
		if not self.socket:
			return False
		
		self.sendMsg(DataPacket(MSG2_GAME_CMD, self.uid, 0, len(msg), msg))
		self.log("game command sent: %s" % (msg))
		
		return True
		

	# Kick a user from the server
	def kick(self, uid, reason="no reason"):
		self.sendChat("!kick %d %s" % (uid, reason))

	# Ban a user from the server
	def ban(self, uid, reason="no reason"):
		self.sendChat("!ban %d %s" % (uid, reason))

	# Say something as host
	def say(self, uid, reason):
		self.sendChat("!say %d %s" % (uid, reason))
	
	# Internal use only!
	# Use sendMsg instead
	def _sendRaw(self, data):
		if self.socket is None:
			return False

		try:
			if data is None:
				return
			self.socket.send(data)
		except Exception, e:
			self.log('sendMsg error: '+str(e))
			self.runCondition = 0
			# import traceback
			# traceback.print_exc(file=sys.stdout)
		return True

	# Internal use only!
	def _packPacket(self, packet):
		if packet.size == 0:
			# just header
			data = struct.pack('IIII', packet.command, packet.source, packet.streamid, packet.size)
		else:
			data = struct.pack('IIII'+str(packet.size)+'s', packet.command, packet.source, packet.streamid, packet.size, str(packet.data))
		return data
		
	def sendMsg(self, packet):
		if self.socket is None:
			return False
		if(packet.command!=MSG2_STREAM_DATA):
			self.log("S>| %-18s %03d:%02d (%d)" % (commandName(packet.command), packet.source, packet.streamid, packet.size))
		#print "S>| %-18s %03d:%02d (%d)" % (commandName(packet.command), packet.source, packet.streamid, packet.size)
		self._sendRaw(self._packPacket(packet))
		
		return True

	def receiveMsg(self, timeout=0.5, block=True):
		try:
			return self.receivedMessages.get(block, timeout)
		except Queue.Empty:
			return None
	
	def _start_receive_thread(self):
		# We need a socket to receive...
		if self.socket is None:
			self.log("Tried to receive on None socket (#ERROR_CON009)")
			return None
		
		self.socket.settimeout(5)
		
		while self.runCondition:
			# get the header	
			data = ""
			tmp = ""
			errorCount = 0
			try:
				while len(data)<self.headersize and self.runCondition:
					try:
						tmp = self.socket.recv(self.headersize-len(data))
					except socket.timeout:
						continue
					
					# unfortunately, we have to do some stupid stuff here, to avoid infinite loops...
					if not tmp:
						errorCount += 1;
						if errorCount > 3:
							# lost connection
							self.log("Connection error #ERROR_CON005")
							self.runCondition = 0
							break
						continue
					else:
						data += tmp
			
				if not data or errorCount > 3:
					# lost connection
					self.log("Connection error #ERROR_CON008")
					self.runCondition = 0
					break
			
				(command, source, streamid, size) = struct.unpack('IIII', data)

				data = ""
				tmp = ""
				while len(data)<size and self.runCondition:
					try:
						tmp = self.socket.recv(size-len(data))
					except socket.timeout:
						continue
					
					# unfortunately, we have to do some stupid stuff here, to avoid infinite loops...
					if not tmp:
						errorCount += 1;
						if errorCount > 3:
							# lost connection
							self.log("Connection error #ERROR_CON006")
							self.runCondition = 0
							break
						continue
					else:
						data += tmp
			except socket.error:
				self.log("Connection error #ERROR_CON015")
				self.runCondition = 0
				break

			if not data or errorCount > 3:
				# lost connection
				self.log("Connection error #ERROR_CON007")
				self.runCondition = 0
				break
			
			if self.dropIncStreamData and command==MSG2_STREAM_DATA:
				continue

			content = struct.unpack(str(size) + 's', data)[0]

			if not command in [MSG2_STREAM_DATA, MSG2_UTF_CHAT, MSG2_NETQUALITY]:
				self.log("R<| %-18s %03d:%02d (%d)" % (commandName(command), source, streamid, size))

			self.receivedMessages.put(DataPacket(command, source, streamid, size, content))
		self.log("Receive thread exiting...")
	
	def setNetQuality(self, quality):
		if self.netQuality != quality:
			self.netQuality = quality
			return True
		else:
			return False

	def getNetQuality(self, quality):
		return self.netQuality