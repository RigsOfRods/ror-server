#!/bin/env python

import sys, time
from connection import *
		
#####################
# GENERAL FUNCTIONS #
#####################

class RoRClient:

	def __init__(self, dropIncStreamData = False):
		# Some settings
		self.dropIncStreamData   = dropIncStreamData
		
		# Initialize some other variables
		self.server = None
		self.secondsPerFrame = 30.0
		self.timeoutWait     = 5.0
		self.currentTime = time.time()
		self.lastFrameTime = time.time()
		
	def connect(self, user, server):
		# Create the server object
		self.server = RoR_Connection(self.dropIncStreamData, self.messageHandler)
		self.user = user
		self.serverinfo = server
						
		# try to connect to the server
		if not self.server.connect(self.user, self.serverinfo):
			self.messageHandler("Couldn't connect to server (#ERROR_CON001)")
			sys.exit()
				
		self.lastFrameTime = time.time()
		
		# event handling
		self.on_connect()
	
	def processOnce(self, block=False):
		if not self.server.isConnected():
			return False

		while True:
			# Are we still connected?
			if not self.server.isConnected():
				self.messageHandler("Lost connection to server (#ERROR_CON003)")
				self.on_disconnect()
				return False

			# Receive a new message
			packet = self.server.receiveMsg(self.timeoutWait, block)
			
			# Execute the framestep once every 30 seconds
			self.currentTime = time.time()
			if self.currentTime-self.lastFrameTime > self.secondsPerFrame:
				# 20 FPS, should be enough to drive a truck fluently
				self.on_frameStep(self.currentTime-self.lastFrameTime)
				self.lastFrameTime = self.currentTime
				
			# Are there any packets left?
			if packet is None:
				return True
			
			# Process the received message
			self.processPacket(packet)
		
	def bigLoop(self):
		# Start the main loop
		while self.server.runCondition:
		
			# Are we still connected?
			if not self.server.isConnected():
				self.messageHandler("Lost connection to server (#ERROR_CON003)")
				break

			# Receive a new message
			packet = self.server.receiveMsg(self.timeoutWait)
			
			# Process the received message
			if not packet is None:	
				self.processPacket(packet)
			
			# Execute the framestep once every 30 seconds
			self.currentTime = time.time()
			if self.currentTime-self.lastFrameTime > self.secondsPerFrame:
				# 20 FPS, should be enough to drive a truck fluently
				self.on_frameStep(self.currentTime-self.lastFrameTime)
				self.lastFrameTime = self.currentTime
			
		# event handling
		self.on_disconnect()

	def processPacket(self, packet):
		if packet.command == MSG2_STREAM_DATA:
			stream = self.server.sm.getStreamData(packet.source, packet.streamid)

			if(stream.type == TYPE_CHARACTER):
				streamData = processCharacterData(packet.data)
				if streamData.command == CHARCMD_POSITION:
					self.server.sm.setPosition(packet.source, packet.streamid, streamData.pos)
					self.server.sm.setCurrentStream(packet.source, packet.source, packet.streamid)
				elif streamData.command == CHARCMD_ATTACH:
					self.server.sm.setCurrentStream(packet.source, streamData.source_id, streamData.stream_id)
				self.on_streamData(packet.source, stream, streamData)
				
			elif(stream.type==TYPE_TRUCK):
				streamData = processTruckData(packet.data)
				self.server.sm.setPosition(packet.source, packet.streamid, streamData.refpos)
				self.on_streamData(packet.source, stream, streamData)

			elif stream == None:
				self.messageHandler("EEE stream %-4s:%-2s not found!" % (packet.source, packet.streamid))

		elif packet.command == MSG2_NETQUALITY:
			quality = processNetQuality(packet.data)
			if self.server.setNetQuality(quality):
				self.on_netQualityChange(packet.source, quality)

		elif packet.command == MSG2_UTF_CHAT:
			if packet.source > 100000:
				packet.source = -1
			str_tmp = str(packet.data).strip('\0')

			# event handling
			if (len(str_tmp) > 0):
				if packet.source==-1:
					self.on_serverMessage(str_tmp)
				elif(packet.source != self.server.uid):
					self.on_publicChat(packet.source, str_tmp)

		elif packet.command == MSG2_STREAM_REGISTER:
			data = processRegisterStreamData(packet.data)
			self.server.sm.addStream(data)
			
			
			if data.type == TYPE_TRUCK:
				# event handling
				res = self.on_streamRegister(packet.source, data)
				
				# Send response
				if res != 1:
					res = -1
				self.server.replyToStreamRegister(data, res)

		elif packet.command == MSG2_USER_INFO or packet.command == MSG2_USER_JOIN:
			data = processUserInfo(packet.data)
			res = self.server.sm.updateClient(data)	
			
			# event handling
			if packet.source!=self.server.uid:
				if res=="updated":
					self.on_userUpdate(packet.source, data)
				if res=="added":
					self.on_userJoin(packet.source, data)

		elif packet.command == MSG2_STREAM_REGISTER_RESULT:
			self.on_streamRegisterResult(packet.source, processRegisterStreamData(packet.data))

		elif packet.command == MSG2_USER_LEAVE:
		
			# event handling
			self.on_userLeave(packet.source)
			
			# Did we leave the server?
			if packet.source == self.server.uid:
				self.messageHandler("Server closed connection (#ERROR_CON010 - server shutdown)")
				self.server.runCondition = 0
			
			# Remove the client that left
			self.server.sm.delClient(packet.source)

		elif packet.command == MSG2_GAME_CMD:
			str_tmp = str(packet.data).strip('\0')
							
			# event handling
			if (len(str_tmp) > 0) and (packet.source != self.server.uid):
				self.on_gameCommand(packet.source, str_tmp)

		elif packet.command == MSG2_UTF_PRIVCHAT:
			str_tmp = str(packet.data).strip('\0')
			self.messageHandler("CHAT| (private) " + str_tmp)
			
			# event handling
			if (len(str_tmp) > 0) and (packet.source != self.server.uid):
				self.on_privateChat(packet.source, str_tmp)

		else:
			str_tmp = str(packet.data).strip('\0')
			self.messageHandler('Unhandled message (type: %d, from: %d): %s' % (packet.command, packet.source, str_tmp))



	def messageHandler(self, msg):
		print(msg)

	
	# Events
	def on_frameStep(self, dt):
		# Keeps the connection open...
		self.server.sendGameCmd("KEEP_ALIVE")
	
	def on_connect(self):
		pass

	def on_userJoin(self, source, user):
		pass

	def on_userUpdate(self, source, user):
		pass
		
	def on_userLeave(self, source):
		pass
	
	def on_publicChat(self, source, message):
		self.messageHandler("CHAT| " + self.server.sm.getUsername(source) + message)

	def on_privateChat(self, source, message):
		pass
	
	def on_serverMessage(self, message):
		self.messageHandler("MSG| " + message)
	
	def on_streamRegister(self, source, stream):
		return -1
	
	def on_streamRegisterResult(self, source, stream):
		pass
	
	def on_gameCommand(self, source, cmd):	
		self.messageHandler("CMD| " + self.server.sm.getUsername(source) + cmd)
		
	def on_disconnect(self):
		pass
	
	def on_streamData(self, source, stream, data):
		pass
	
	def on_netQualityChange(self, source, quality):
		if quality==1:
			self.messageHandler("Connection problem detected.")
		elif quality==0:
			self.messageHandler("Connection problem resolved.")

			
			
			
if __name__ == "__main__":
	# Set our information
	user                = user_info_t()
	user.username       = "Services"
	user.usertoken      = ""
	user.serverpassword = "secret"
	user.language       = "nl_BE"
	user.clientname     = "RoRbot"
	user.clientversion  = "1"
	
	# Set the server information
	serverinfo          = server_info_t()
	serverinfo.host     = "127.0.0.1"
	serverinfo.port     = 12000
	
	# Create the client
	client = RoRClient()
	
	# Connect
	client.connect(user, serverinfo)
	
	# Keep running
	client.bigLoop()