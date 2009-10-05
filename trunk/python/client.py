#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
# updated for rornet_2.3 October 2009

import sys, struct, threading, socket, random, time, string, os, os.path, time, math, copy
from rornet import *
from utils import *


restartClient = True
restartCommands = ['!connect'] # important ;)
eventStopThread = None

class Client(threading.Thread):
	uid = 0
	oclients = {}
	
	def __init__(self, ip, port, client_id, commands):
		self.ip = ip
		self.port = port
		self.client_id = client_id
		self.startupCommands = commands

		sys.stdout = Logger(client_id)
		print "logger started!"
		self.runCond = True
		self.username = ''
		self.socket = None

		threading.Thread.__init__(self)

	def processCommand(self, cmd, packet=None, startup=False):
		global restartClient, restartCommands, eventStopThread
		#print "processCommand: %s" % (cmd)
		if cmd == "!rejoin":
			eventStopThread.set()
			self.playback.join(1)
			self.disconnect()
			restartCommands.append('!say rejoined')
			self.runCond = False
		elif cmd[:4] == "!say":
			self.sendChat(cmd[4:].strip())
		elif cmd == "!shutdown":
			if not self.playback is None:
				eventStopThread.set()
				self.playback.join(1)
			self.disconnect()
			sys.exit(0)
		elif cmd == "!connect":
			self.connect()
		elif cmd == "!disconnect":
			if not self.playback is None:
				eventStopThread.set()
				self.playback.join(1)
			self.disconnect()
		elif cmd == "!ping":
			self.sendChat("pong!")
		else:
			if cmd[0] == "!":
				print 'command not found: %s' % cmd
		
	def disconnect(self):
		if not self.socket is None:
			self.sendMsg(DataPacket(MSG2_DELETE, 0, 0, 0))
			print 'closing socket'
			self.socket.close()
		self.socket = None
	
	def connect(self):
		print 'creating socket'
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		print 'connecting to server %s:%d' %(self.ip, self.port)
		self.socket.connect((self.ip, self.port))

		self.sendMsg(DataPacket(MSG2_HELLO, 0, 0, len(RORNET_VERSION), RORNET_VERSION))

		packet = self.receiveMsg()
		if packet.command != MSG2_VERSION:
			print 'invalid handshake: MSG2_VERSION'
			self.disconnect()
			return
		version = packet.data
		
		packet = self.receiveMsg()
		if packet.command != MSG2_TERRAIN_RESP:
			print 'invalid handshake: MSG2_TERRAIN_RESP'
			self.disconnect()
			return

		self.terrain = packet.data
		
		data = struct.pack('20s40s40s', self.username, self.password, self.uniqueid)
		self.sendMsg(DataPacket(MSG2_USER_CREDENTIALS, 0, 0, len(data), data))

		packet = self.receiveMsg()
		if packet.command != MSG2_WELCOME:
			print 'invalid handshake: MSG2_WELCOME'
			self.disconnect()
			return
		
		self.sendMsg(DataPacket(MSG2_USE_VEHICLE, 0, 0, len(self.truckname), self.truckname))
		
		data = struct.pack('I', self.buffersize)
		self.sendMsg(DataPacket(MSG2_BUFFER_SIZE, 0, 0, len(data), data))
					
	def run(self):
		# some default values
		self.uniqueid = "1337"
		self.password = ""
		self.username = "GameBot_"+str(self.client_id)
		self.buffersize = 1 # 'data' - String
		self.truckname = "spectator"
		
		# dummy data to prevent timeouts
		dummydata = self.packPacket(DataPacket(MSG2_VEHICLE_DATA, 0, 0, 1, "1"))

		while self.runCond:
			if len(self.startupCommands) > 0:
				#print "startupCommands: ", startupCommands
				cmd = self.startupCommands.pop(0).strip()
				if cmd != "":
					print 'executing startup command %s' % cmd
					self.processCommand(cmd, None, True)
				repeat = (len(startupCommands) > 0)
		
			packet = self.receiveMsg()
			if not packet is None:
				# record the used vehicles
				if packet.command == MSG2_USE_VEHICLE:
					data = str(packet.data).split('\0')
					self.oclients[packet.source] = data
				
				if packet.command == MSG2_CHAT:
					self.processCommand(str(packet.data), packet)
		
		
			if True: #self.mode == MODE_NORMAL:
				# to prevent timeouts in normal mode
				self.sendRaw(dummydata)
		

		
	def sendChat(self, msg):
		maxsize = 50
		if len(msg) > maxsize:
			for i in range(0, math.ceil(float(len(msg)) / float(maxsize))):
				if i == 0:
					msga = msg[maxsize*i:maxsize*(i+1)]
					self.sendMsg(DataPacket(MSG2_CHAT, 0, 0, len(msga), msga))
				else:
					msga = "| "+msg[maxsize*i:maxsize*(i+1)]
					self.sendMsg(DataPacket(MSG2_CHAT, 0, 0, len(msga), msga))
		else:
			self.sendMsg(DataPacket(MSG2_CHAT, 0, 0, len(msg), msg))
		
	def sendRaw(self, data):
		if self.socket is None:
			print 'cannot send: not connected!'
			return
		try:
			self.socket.send(data)
		except Exception, e:
			print 'sendMsg error: '+str(e)
			import traceback
			traceback.print_exc(file=sys.stdout)

	def packPacket(self, packet):
		if packet.size == 0:
			# just header
			data = struct.pack('IIII', packet.command, packet.source, packet.streamid, packet.size)
		else:
			data = struct.pack('IIII'+str(packet.size)+'s', packet.command, packet.source, packet.streamid, packet.size, str(packet.data))
		return data
		
	def sendMsg(self, packet):
		if self.socket is None:
			return
		print "SEND| %-16s, source %d, destination %d, size %d, data-len: %d" % (commandName(packet.command), packet.source, self.uid, packet.size, len(str(packet.data)))
		self.sendRaw(self.packPacket(packet))

	def receiveMsg(self):
		if self.socket is None:
			return None
		note = ""
		headersize = struct.calcsize('IIII')
		data = ""
		readcounter = 0
		while len(data) < headersize:
			data += self.socket.recv(headersize-len(data))
		if readcounter > 1:
			note += "HEADER SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		(command, source, streamid, size) = struct.unpack('IIII', data)
		
		data = ""
		readcounter = 0
		while len(data) < size:
			readcounter+=1
			data += self.socket.recv(size-len(data))
		if readcounter > 1:
			note += "DATA SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		content = struct.unpack(str(size) + 's', data)
		content = content[0]

		if not command in [MSG2_VEHICLE_DATA, MSG2_CHAT]:
			print "RECV| %-16s, source %d, size %d, data-len: %d -- %s" % (commandName(command), self.uid, size, len(content), note)
		return DataPacket(command, source, streamid, size, content)

if __name__ == '__main__':
	eventStopThread = threading.Event()
	ip = sys.argv[1]
	port = int(sys.argv[2])
	startupCommands = ['!connect', '!say hello!']
	if len(sys.argv) == 5:
		startupCommands = sys.argv[4].split(';')
	
	num = 1
	threads = []
	restarts = {}
	lastrestart = {}
	try:
		# try for keyboard interrupt
		for i in range(num):
			threads.append(Client(ip, port, i, copy.copy(startupCommands)))
			threads[i].start()
			lastrestart[i] = time.time() - 1000
			restarts[i] = 0
			# start with time inbetween, so you see all trucks ;)
			time.sleep(2.0)

		print "all threads started. starting restart loop"
		time.sleep(1)
	
		while restartClient:
			eventStopThread.clear()
			for i in range(num):
				if not threads[i].isAlive():
					restarts[i]+=1
					rcmd = copy.copy(restartCommands)
					print "restart commands: ", rcmd
					if time.time() - lastrestart[i] < 1:
						rcmd = ['!connect', '!say i crashed, resetted to normal mode :|']
					print "thread %d dead, restarting" % i
					threads[i] = Client(ip, port, i, rcmd)
					threads[i].start()
					lastrestart[i] = time.time()
					
				else:
					time.sleep(1)
				#	print "thread %d alive!" % i
		print "exiting peacefully"
	except KeyboardInterrupt:
		print "exiting ..."
		sys.exit(0)
