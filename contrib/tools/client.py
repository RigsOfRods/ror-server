#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
# updated for rornet_2.3 October 2009

import sys, struct, threading, thread, socket, random, time, string, os, os.path, time, math, copy
from rornet import *
from utils import *

defaultCommands = "!connect;!reguser;!userdata"
restartClient = True
restartCommands = ['!connect'] # important ;)
eventStopThread = None

class GlobalStateTracker:
	clientLock = thread.allocate_lock()
	streams = {}
	
	def addStream(self, uid, streamid, data):
		sid = '%03d:%02d' % (int(uid), int(streamid))
		with self.clientLock:
			self.streams[sid] = (uid, streamid, data)

	def addClient(self, uid, username, password, uniqueid):
		self.addStream(uid, -1, (username, password, uniqueid))

	def removeStream(self, uid, streamid):
		sid = '%03d:%02d' % (int(uid), int(streamid))
		with self.clientLock:
			if sid in self.streams.keys():
				del self.streams[sid]

	def removeClient(self, uid, username, password, uniqueid):
		self.removeStream(uid, -1)
		

class Client(threading.Thread):
	uid = 0
	oclients = {}
	
	def __init__(self, tracker, ip, port, client_id, commands):
		self.tracker = tracker
		self.ip = ip
		self.port = port
		self.client_id = client_id
		self.startupCommands = commands

		sys.stdout = Logger(client_id)
		print "logger started!"
		self.runCond = True
		self.username = ''
		self.socket = None
		self.headersize = struct.calcsize('IIII')

		threading.Thread.__init__(self)

	def processCommand(self, cmd, packet=None, startup=False):
		global restartClient, restartCommands, eventStopThread
		if cmd[0] == "!":
			print "EX| %s" % (cmd)
		if cmd == "!rejoin":
			eventStopThread.set()
			self.disconnect()
			restartCommands.append('!say rejoined')
			self.runCond = False
		elif cmd[:4] == "!say":
			self.sendChat(cmd[4:].strip())
		elif cmd == "!shutdown":
			self.disconnect()
			sys.exit(0)
		elif cmd == "!connect":
			self.connect()
		elif cmd == "!disconnect":
			self.disconnect()
		elif cmd == "!ping":
			self.sendChat("pong!")
		elif cmd == "!reguser":
			sid = 11
			stype = 1
			sstatus = 1
			sname = "example stream"
			data = registerStreamData(sid, sname, stype, sstatus)
			self.sendMsg(DataPacket(MSG2_STREAM_REGISTER, self.uid, sid, len(data), data))
			self.tracker.addStream(self.uid, sid, (sid, sname, stype, sstatus))
		elif cmd == "!userdata":
			sid = 11
			data = "test123" # vector and such would be here
			self.sendMsg(DataPacket(MSG2_STREAM_DATA, self.uid, sid, len(data), data))
		else:
			if cmd[0] == "!":
				print 'command not found: %s' % cmd
		
	def disconnect(self):
		if not self.socket is None:
			self.sendMsg(DataPacket(MSG2_DELETE, 0, 0, 0, 0))
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
		
		# receive own user data
		packet = self.receiveMsg()
		if packet.command != MSG2_USER_JOIN:
			print 'invalid handshake: MSG2_USER_JOIN'
			self.disconnect()
			return
		self.uid = packet.source
		(version, self.nickname, self.authstatus, self.slotid) = struct.unpack('c20sii', packet.data)
		self.nickname = self.nickname.strip()
		print "joined as '%s' on slot %d with UID %d with auth %d" % (self.nickname, self.slotid, self.uid, self.authstatus)
		self.tracker.addClient(self.uid, self.username, self.password, self.uniqueid)
		return True

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
					#print 'executing startup command %s' % cmd
					self.processCommand(cmd, None, True)
				repeat = (len(startupCommands) > 0)
		
			packet = self.receiveMsg()
			if not packet is None:
				# record the used vehicles
				if packet.command == MSG2_USE_VEHICLE:
					data = str(packet.data).split('\0')
					self.oclients[packet.source] = data
				
				if packet.command == MSG2_CHAT and packet.source != self.uid: # ignore chat from ourself
					print "CH| " + str(packet.data)
					self.processCommand(str(packet.data), packet)
		
				if packet.command == MSG2_STREAM_REGISTER:
					processStreamRegisterData(packet.data)
					
				if packet.command == MSG2_USER_INFO:
					processUserOnJoinInfo(packet.data)
					
					
		
			if True: #self.mode == MODE_NORMAL:
				# to prevent timeouts in normal mode
				self.sendRaw(dummydata)
		

		
	def sendChat(self, msg):
		if not self.socket:
			self.connect()
		
		# word wrap size	
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
			self.connect()

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
		print "S>| %-18s %03d:%02d (%d)" % (commandName(packet.command), packet.source, packet.streamid, packet.size)
		self.sendRaw(self.packPacket(packet))

	def receiveMsg(self):
		if self.socket is None:
			return None
		note = ""
		
		# we set the timeout so low, so our receive end is not blocking the other processing (could be fixed with using a separate send thread)
		self.socket.settimeout(0.5)
		
		data = ""
		readcounter = 0
		while len(data) < self.headersize:
			try:
				data += self.socket.recv(self.headersize-len(data))
			except socket.timeout:
				self.socket.settimeout(None)
				return
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
			print "R<| %-18s %03d:%02d (%d)%s" % (commandName(command), source, streamid, size, note)
		return DataPacket(command, source, streamid, size, content)

if __name__ == '__main__':
	eventStopThread = threading.Event()
	if len(sys.argv) < 2:
		print "usage: %s 127.0.0.1 12000" % sys.argv[0]
		sys.exit(0)
	ip = sys.argv[1]
	port = int(sys.argv[2])
	startupCommands = defaultCommands.split(';')

	if len(sys.argv) > 3:
		startupCommands = (' '.join(sys.argv[3:])).split(';')
	
	stateTracker = GlobalStateTracker()
	num = 10
	threads = []
	restarts = {}
	lastrestart = {}
	try:
		# try for keyboard interrupt
		for i in range(num):
			threads.append(Client(stateTracker, ip, port, i, copy.copy(startupCommands)))
			threads[i].start()
			lastrestart[i] = time.time() - 1000
			restarts[i] = 0
			# start with time inbetween, so you see all trucks ;)
			time.sleep(2.0)

		#print "all threads started. starting restart loop"
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
					# check if the information in all clients is in sync
					
					time.sleep(1)
				#	print "thread %d alive!" % i
		print "exiting peacefully"
	except KeyboardInterrupt:
		print "exiting ..."
		sys.exit(0)
