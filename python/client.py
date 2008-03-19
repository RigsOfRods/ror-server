#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
import sys, struct, logging, threading, socket, random, time, string, os, os.path, time, math, copy

logging.basicConfig(level=logging.DEBUG, format='%(name)s: %(message)s')

MSG2_HELLO = 1000
MSG2_VERSION = 1001
MSG2_FULL = 1002
MSG2_BANNED = 1003
MSG2_WELCOME = 1004
MSG2_USE_VEHICLE = 1005
MSG2_SPAWN = 1006
MSG2_BUFFER_SIZE = 1007
MSG2_VEHICLE_DATA = 1008
MSG2_USER = 1009
MSG2_DELETE = 1010
MSG2_CHAT = 1011
MSG2_FORCE = 1012
MSG2_USER_CREDENTIALS = 1017
MSG2_TERRAIN_RESP = 1019
MSG2_WRONG_PW = 1020
MSG2_RCON_LOGIN = 1021
MSG2_RCON_LOGIN_FAILED = 1022
MSG2_RCON_LOGIN_SUCCESS = 1023
MSG2_RCON_LOGIN_NOTAV = 1024
MSG2_RCON_COMMAND = 1025
MSG2_RCON_COMMAND_FAILED = 1026
MSG2_RCON_COMMAND_SUCCESS = 1027

commandNames= {
	1000:"HELLO",
	1001:"VERSION",
	1002:"FULL",
	1003:"BANNED",
	1004:"WELCOME",
	1005:"USE_VEHICLE",
	1006:"SPAWN",
	1007:"BUFFER_SIZE",
	1008:"VEHICLE_DATA",
	1009:"USER",
	1010:"DELETE",
	1011:"CHAT",
	1012:"FORCE",
	1017:"USER_CREDENTIALS",
	1019:"TERRAIN_RESP",
	1020:"WRONG_PW",
}

RORNET_VERSION = "RoRnet_2.1"
 
restartClient=True
restartCommands=['!connect'] # important ;)
eventStopThread = None

MODE_NORMAL = 0
MODE_PLAYBACK = 1
MODE_RECORD = 1

class DataPacket:
	source=0
	command=0
	size=0
	data=0
	time=0
	def __init__(self, command, source, size, data):
		self.source = source
		self.command = command
		self.size = size
		self.data = data
		self.time = 0

class Recording:
	def __init__(self):
		self.vehicle = ""
		self.terrain = ""
		self.buffersize = 0
		self.username = ""
		self.playbackposition = 0
		self.list = []
		self.starttime = 0

		
class Playback(threading.Thread):
	def __init__(self, client, record, logger):
		self.runcondition=True
		self.client = client
		self.record = record
		self.logger = logger
		threading.Thread.__init__(self)
		
	def getNextRecordedData(self, record):
		if len(record.list) == 0:
			return (None, 0.1)
		notfound = True
		pos = record.playbackposition
		loops=0
		pos += 1
		while True:
			if record.list[pos].command in [MSG2_VEHICLE_DATA]: # , MSG2_CHAT
				break
			else:
				self.logger.debug("command not suitable, left out: %s"  % commandNames[record.list[pos].command])
			pos += 1
			if pos >= len(record.list):
				loops+=1
				pos = 0
			if loops > 2:
				# would loop forever otherwise
				self.runCond=False
				return
		record.playbackposition = pos
		#self.logger.debug("played back position %d" % pos)
		packet = record.list[pos]
		
		nextpos = pos+1
		t = 0.2
		if nextpos >= len(record.list):
			nextpos = 0
			t = 2
			
		nextpacket = record.list[nextpos]
		if nextpos < len(record.list):
			t = nextpacket.time - packet.time
		
		if t > 1: 
			t = 0.2
		if t < 0:
			t = 1

		#self.logger.debug("sleep time: %f" % t)
		self.logger.debug("played back position %d, frame time: %0.4f" % (pos, t))

		packet.source = 0
		return (packet, t)
		
	def run(self):
		print "playback thread started"
		global eventStopThread
		while not eventStopThread.isSet():
			(packet, sleeptime) = self.getNextRecordedData(self.record)
			if not packet is None:
				self.client.sendMsg(packet)
				time.sleep(sleeptime)
		print "playback thread ended"
	
class Client(threading.Thread):
	uid = 0
	oclients = {}
	
	def __init__(self, ip, port, cid, restartnum, commands):
		self.ip = ip
		self.port = port
		self.cid = cid
		self.restartnum = restartnum
		self.logger = logging.getLogger('client-%02d' % cid)
		self.logger.debug('__init__')
		self.runCond = True
		self.startupCommands = commands
		self.username = ''
		self.socket = None
		self.playback = None
		
		self.mode = MODE_NORMAL
		self.recordmask = []
		self.record = None
		
		threading.Thread.__init__(self)

	def recordNow(self):
		if not self.recordmask[0] in self.oclients.keys():
			self.logger.debug('player %d does not exist to be recorded!', self.recordmask[0])
			return

		self.record = Recording()
		self.record.vehicle = self.oclients[self.recordmask[0]][0]
		self.record.username = self.oclients[self.recordmask[0]][1]
		self.record.terrain = self.terrain
		self.sendChat("recording player %d (%s, %s)  ..." % (self.recordmask[0], self.record.vehicle, self.record.username))
		self.mode = MODE_RECORD
	
	def processCommand(self, cmd, packet=None):
		global restartClient, restartCommands, eventStopThread
		self.logger.debug("%s / %d" % (cmd, self.mode))
		if cmd == "!recordme" and self.mode == MODE_NORMAL:
			self.recordmask = [packet.source]
			self.recordNow()
		elif cmd[:8] == "!record " and self.mode == MODE_NORMAL:
			self.recordmask = [int(cmd[8:])]
			self.recordNow()
		elif cmd == "!recordme" and self.mode == MODE_RECORD:
			self.sendChat("already recoding, only one recording a time possible")
		elif cmd == "!stop" and self.mode == MODE_RECORD and packet.source in self.recordmask:
			fn = self.newRecordname()
			self.saveRecording(fn, self.record)
			self.sendChat("saved recording as %s" % os.path.basename(fn))
			self.mode = MODE_NORMAL
			self.recordmask = []
		elif cmd == "!records" and self.mode == MODE_NORMAL:
			recs = self.getAvailableRecordings()
			if len(recs) == 0:
				msg = "no recordings available!"
				self.sendChat(msg)
			else:
				msg = "available recordings: " + ', '.join(recs)
				self.sendChat(msg)
		elif cmd[:8] == "!playrec" and self.mode == MODE_NORMAL:
			playbackname = cmd[8:].strip()
			if not self.socket is None:
				# rejoin to be able to play back
				if self.recordExists(playbackname + ".rec"):
					#self.sendChat("recording '%s' does exist!" % playbackname)
					restartCommands.append("!playrec %s"%playbackname)
					self.runCond=False
				else:
					msg = "recording '%s' does not exist!" % playbackname
					self.logger.debug(msg)
					self.sendChat(msg)
			elif self.socket is None:
				# rejoined
				if self.loadRecord(playbackname + ".rec"):
					self.sendChat("playing recording %s ..." % playbackname)
					self.playback = Playback(self, self.record, self.logger)
					self.connect()
					self.playback.start()
		
		elif cmd == "!rejoin":
			eventStopThread.set()
			self.playback.join(1)
			self.disconnect()
			restartCommands = ['!connect', '!say rejoined']
			self.runCond = False
		elif cmd == "!stresstest" and self.mode == MODE_NORMAL:
			#stressTest = not stressTest
			#if stressTest:
			#	self.sendChat("stressTest enabled, with buffersize of %d"%buffersize)
			#else:
			#	self.sendChat("stressTest disabled")
			# XXX: to reimplement
			pass
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
				self.logger.debug('command not found: %s' % cmd)
		
	def loadRecord(self, playbackFile):
		self.logger.debug('trying to load recording %s' % playbackFile)
		self.record = self.loadRecording(playbackFile)
		self.logger.debug('# loaded record: %d' % len(self.record.list))
		self.logger.debug('# %s, %s' % (self.record.vehicle, self.record.username))
		if len(self.record.list) == 0:
			self.logger.debug('nothing to play back!')
			return False
		self.username = "[REC]"+str(self.record.username[:14])
		self.buffersize = self.record.list[0].size
		self.truckname = self.record.vehicle
		return True

	def disconnect(self):
		if not self.socket is None:
			self.sendMsg(DataPacket(MSG2_DELETE, 0, 0, 0))
			self.logger.debug('closing socket')
			self.socket.close()
		self.socket = None
	
	def connect(self):
		self.logger.debug('creating socket')
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.logger.debug('connecting to server')
		self.socket.connect((self.ip, self.port))

		self.sendMsg(DataPacket(MSG2_HELLO, 0, len(RORNET_VERSION), RORNET_VERSION))
		packet = self.receiveMsg()
		if packet.command != MSG2_VERSION:
			self.logger.debug('invalid handshake: MSG2_VERSION')
			self.disconnect()
			return
		version = packet.data
		
		packet = self.receiveMsg()
		if packet.command != MSG2_TERRAIN_RESP:
			self.logger.debug('invalid handshake: MSG2_TERRAIN_RESP')
			self.disconnect()
			return

		self.terrain = packet.data
		
		data = struct.pack('20s40s40s', self.username, self.password, self.uniqueid)
		self.sendMsg(DataPacket(MSG2_USER_CREDENTIALS, 0, len(data), data))

		packet = self.receiveMsg()
		if packet.command != MSG2_WELCOME:
			self.logger.debug('invalid handshake: MSG2_WELCOME')
			self.disconnect()
			return
		
		self.sendMsg(DataPacket(MSG2_USE_VEHICLE, 0, len(self.truckname), self.truckname))
		
		data = struct.pack('I', self.buffersize)
		self.sendMsg(DataPacket(MSG2_BUFFER_SIZE, 0, len(data), data))
					
	def run(self):
		# some default values
		self.uniqueid = "1337"
		self.password = ""
		self.username = "GameBot_"+str(self.cid)
		self.buffersize = 1
		self.truckname = "spectator"
			
		while self.runCond:
			if len(self.startupCommands) > 0:
				cmd = self.startupCommands.pop(0).strip()
				if cmd != "":
					self.logger.debug('executing startup command %s' % cmd)
					self.processCommand(cmd)
				repeat = (len(startupCommands) > 0)
		
			packet = self.receiveMsg()
			if not packet is None:
				# record the used vehicles
				if packet.command == MSG2_USE_VEHICLE:
					data = str(packet.data).split('\0')
					self.oclients[packet.source] = data
				
				if self.mode == MODE_RECORD and packet.source in self.recordmask and packet.command in [MSG2_VEHICLE_DATA, MSG2_CHAT]:
					packet.time = time.time()
					self.record.buffersize = packet.size
					self.record.list.append(packet)
					self.logger.debug('recorded frame %d of client %d, buffersize %d' % (len(self.record.list), packet.source, packet.size))
				
				if self.mode == MODE_RECORD and packet.source in self.recordmask and packet.command == MSG2_DELETE:
					fn = self.newRecordname()
					self.saveRecording(fn, self.record)
					self.sendChat("saved recording as %s (client exited)" % os.path.basename(fn))
					self.mode = MODE_NORMAL
					
				if self.mode == MODE_RECORD and len(self.record.list) > 1000:
					fn = self.newRecordname()
					self.saveRecording(fn, self.record)
					self.sendChat("saved recording as %s (recording limit reached)" % os.path.basename(fn))
					self.mode = MODE_NORMAL
				
				if packet.command == MSG2_CHAT:
					self.processCommand(str(packet.data), packet)
		
		


	def saveRecording(self, filename, record):
		import pickle
		file = open(filename, 'wb')
		pickle.dump(record, file)
		file.close
		
		self.logger.debug('record saved')

	def loadRecording(self, filename):
		try:
			self.logger.debug('loading record %s ...' % filename)
			import pickle
			file = open(filename, 'rb')
			loaded = pickle.load(file)
			file.close		
			self.logger.debug('record loaded with %d frames!' % (len(loaded.list)))
			return loaded
		except Exception, e:
			self.logger.debug('error: '+str(e))
			self.logger.debug('error while loading recording')
		
	def sendChat(self, msg):
		maxsize = 50
		if len(msg) > maxsize:
			for i in range(0, math.ceil(float(len(msg)) / float(maxsize))):
				if i == 0:
					msga = msg[maxsize*i:maxsize*(i+1)]
					self.sendMsg(DataPacket(MSG2_CHAT, 0, len(msga), msga))
				else:
					msga = "| "+msg[maxsize*i:maxsize*(i+1)]
					self.sendMsg(DataPacket(MSG2_CHAT, 0, len(msga), msga))
		else:
			self.sendMsg(DataPacket(MSG2_CHAT, 0, len(msg), msg))
		
	def newRecordname(self):
		exists = True
		filename = ''
		num=0
		while exists:
			#filename = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rec%d.rec'%num)
			filename = 'rec%d.rec' % num
			exists = os.path.isfile(filename)
			num+=1
		return filename

	def recordExists(self, filename):
		return os.path.isfile(filename)
		
	def getAvailableRecordings(self):
		import glob
		return glob.glob('*.rec')
		
	def sendRaw(self, data):
		if self.socket is None:
			self.logger.debug('cannot send: not connected!')
			return
		try:
			self.socket.send(data)
		except Exception, e:
			self.logger.debug('sendMsg error: '+str(e))
			import traceback
			traceback.print_exc(file=sys.stdout)
		

	def sendMsg(self, packet):
		if self.socket is None:
			return
		self.logger.debug("SEND| %-16s, source %d, destination %d, size %d, data-len: %d" % (commandNames[packet.command], packet.source, self.uid, packet.size, len(str(packet.data))))
		if packet.size == 0:
			# just header
			data = struct.pack('III', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('III'+str(packet.size)+'s', packet.command, packet.source, packet.size, str(packet.data))
		self.sendRaw(data)

	def receiveMsg(self):
		if self.socket is None:
			return None
		note = ""
		headersize = struct.calcsize('III')
		data = ""
		readcounter = 0
		while len(data) < headersize:
			data += self.socket.recv(headersize-len(data))
		if readcounter > 1:
			note += "HEADER SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		(command, source, size) = struct.unpack('III', data)
		
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
			self.logger.debug("RECV| %-16s, source %d, size %d, data-len: %d -- %s" % (commandNames[command], self.uid, size, len(content), note))
		return DataPacket(command, source, size, content)

if __name__ == '__main__':
	eventStopThread = threading.Event()
	ip = sys.argv[1]
	port = int(sys.argv[2])
	num = int(sys.argv[3])
	startupCommands = ['!connect', '!say hello!']
	if len(sys.argv) == 5:
		startupCommands = sys.argv[4].split(';')
	
	try:
		# try for keyboard interrupt
		threads = []
		restarts = {}
		for i in range(num):
			threads.append(Client(ip, port, i, 0, copy.copy(startupCommands)))
			threads[i].start()
			lastrestart[i] = time.time() - 1000
			restarts[i] = 0
			# start with time inbetween, so you see all trucks ;)
			time.sleep(0.2)

	
		print "all threads started. starting restart loop"
		time.sleep(1)
	
		lastrestart = {}
		while restartClient:
			eventStopThread.clear()
			for i in range(num):
				if not threads[i].isAlive():
					restarts[i]+=1
					rcmd = copy.copy(restartCommands)
					if time.time() - lastrestart[i] < 5:
						rcmd = ['!connect', '!say i crashed, resetted to normal mode :|']
					print "thread %d dead, restarting" % i
					threads[i] = Client(ip, port, i, restarts[i], rcmd)
					threads[i].start()
					lastrestart[i] = time.time()
					
				#else:
				#	print "thread %d alive!" % i
	except KeyboardInterrupt:
		print "exiting ..."
		sys.exit(0)
			
			
# old code (to be added):
"""
		buffersize = random.randint(500, 1000)
		if playbackFile != '':
			buffersize = self.record.buffersize
			
		data = ""
		for i in range(buffersize):
			data += random.choice(string.letters + string.digits)
"""