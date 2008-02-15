#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
import sys, struct, logging, threading, socket, random, time, string, os, os.path, time

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
restartClient = True
playbackFile = ""

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

class Client(threading.Thread):
	uid = 0
	oclients = {}
	
	def __init__(self, ip, port, cid, restartnum):
		self.ip = ip
		self.port = port
		self.cid = cid
		self.restartnum = restartnum
		self.logger = logging.getLogger('client')
		self.logger.debug('__init__')
		threading.Thread.__init__(self)

		
	def run(self):
		global restartClient, playbackFile
		record = None
		self.logger.debug('creating socket')
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.logger.debug('connecting to server')
		
		self.socket.connect((self.ip, self.port))

		self.sendMsg(DataPacket(MSG2_HELLO, 0, len(RORNET_VERSION), RORNET_VERSION))
		packet = self.receiveMsg()
		if packet.command != MSG2_VERSION:
			self.logger.debug('invalid handshake: MSG2_VERSION')
			return
		version = packet.data
		
		packet = self.receiveMsg()
		if packet.command != MSG2_TERRAIN_RESP:
			self.logger.debug('invalid handshake: MSG2_VERSION')
			return
		
		if playbackFile != '':
			self.logger.debug('trying to play back %s' % playbackFile)
			record = self.loadRecording(playbackFile)
			self.logger.debug('# loaded record: %d' % len(record.list))
			self.logger.debug('# %s, %s' % (record.vehicle, record.username))
			if len(record.list) == 0:
				self.logger.debug('nothing to play back!')
				playbackFile = ''
				return
			
		
		terrain = packet.data
		
		username = "ATC_"+str(self.cid)
		if playbackFile != '':
			username = "rec_"+str(record.username[:14])
			
		password = ""
		uniqueid = "1337"
		buffersize = random.randint(500, 1000)
		if playbackFile != '':
			buffersize = record.buffersize
		
		data = struct.pack('20s40s40s', username, password, uniqueid)
		self.sendMsg(DataPacket(MSG2_USER_CREDENTIALS, 0, len(data), data))

		packet = self.receiveMsg()
		if packet.command != MSG2_WELCOME:
			self.logger.debug('invalid handshake: MSG2_WELCOME')
			return
		
		truckname = "spectator"
		if playbackFile != '':
			truckname = record.vehicle
		self.sendMsg(DataPacket(MSG2_USE_VEHICLE, 0, len(truckname), truckname))
		
		data = struct.pack('I', buffersize)
		self.sendMsg(DataPacket(MSG2_BUFFER_SIZE, 0, len(data), data))
		
		#self.sendChat("hi, this is my %d. start!" % self.restartnum)
		if playbackFile != '':
			self.sendChat("playing recording %s ..." % playbackFile)
	
		data = ""
		for i in range(buffersize):
			data += random.choice(string.letters + string.digits)

		recordnum = -1
		run = True
		stressTest = False
		while run:
			if playbackFile != '':
				(packet, sleeptime) = self.getNextRecordedData(record)
				if not packet is None:
					self.sendMsg(packet)
					time.sleep(sleeptime)

			if stressTest:
				# send random data
				self.sendMsg(DataPacket(MSG2_VEHICLE_DATA, 0, len(data), data))
				time.sleep(0.2)

			# record the used vehicles
			if packet.command == MSG2_USE_VEHICLE:
				data = str(packet.data).split('\0')
				self.oclients[packet.source] = data
				
			packet = self.receiveMsg()
			if recordnum>=0 and packet.source == recordnum and packet.command == MSG2_VEHICLE_DATA:
				packet.time = time.time()
				record.buffersize = packet.size
				record.list.append(packet)
				self.logger.debug('recorded frame %d' % len(record.list))
				
			if packet.command == MSG2_CHAT:
				print packet.source, recordnum
				# check for commands
				if str(packet.data) == "!recordme" and recordnum<0:
					record = Recording()
					recordnum = packet.source
					record.vehicle = self.oclients[recordnum][0]
					record.username = self.oclients[recordnum][1]
					record.terrain = terrain
					self.sendChat("recording player %d (%s, %s)  ..." % (recordnum, record.vehicle, record.username))
				elif str(packet.data) == "!recordme" and recordnum>=0:
					self.sendChat("already recoding, only one recording a time possible")
				elif str(packet.data) == "!stop" and recordnum>=0 and packet.source == recordnum:
					fn = self.newRecordname()
					self.saveRecording(fn, record)
					self.sendChat("saved recording as %s" % os.path.basename(fn))
					recordnum = -1
				elif str(packet.data) == "!records":
					recs = self.getAvailableRecordings()
					if len(recs) == 0:
						msg = "no recordings available!"
						self.sendChat(msg)
					else:
						msg = "available recordings: " + ', '.join(recs)
						self.sendChat(msg)
				elif str(packet.data)[:9] == "!playback":
					playbackname = str(packet.data)[9:].strip() + ".rec"
					if self.recordExists(playbackname):
						playbackFile = playbackname 
						run=False
					else:
						self.sendChat("recording '%s' does not exist!" % playbackname)
				elif str(packet.data) == "!rejoin":
					playbackFile = ''
					run = False
				elif str(packet.data) == "!stresstest":
					stressTest = not stressTest
					if stressTest:
						self.sendChat("stressTest enabled, with buffersize of %d"%buffersize)
					else:
						self.sendChat("stressTest disabled")
					
				elif str(packet.data) == "!shutdown":
					restartClient=False
					sys.exit(0)
				#elif str(packet.data) == "!lt":
				#	self.sendChat("stressTest disabledsgjasdkf haiskdgh sdkf hsadi,f hkgsb kfghsadbi fkgb 43iytrgfkdyiukw3qyuh458ir743 yetryg8i 4i35 47 iweryfhiwreku stdygtfuyker sdtyhf ni43qukweyth f7i4uk3w ythilifreukyhsdufyfoi ydf9lul hsdf")
				
		
		self.logger.debug('closing socket')
		self.socket.close()


	def saveRecording(self, filename, record):
		import pickle
		file = open(filename, 'wb')
		pickle.dump(record, file)
		file.close
		
		self.logger.debug('record saved')

	def loadRecording(self, filename):
		try:
			self.logger.debug('loading record...')
			import pickle
			file = open(filename, 'rb')		
			loaded = pickle.load(file)
			file.close
			print loaded.list
			print loaded.username
			print loaded.vehicle
			
			self.logger.debug('record loaded with %d frames!' % (len(loaded.list)))
			return loaded
		except:
			pass
			self.logger.debug('error while loading recording')
		
	def sendChat(self, msg):
		maxsize = 50
		if len(msg) > maxsize:
			for i in range(0, int(float(len(msg)) / float(maxsize))):
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
		try:
			self.socket.send(data)
		except Exception, e:
			self.logger.debug('sendMsg error: '+str(e))
			import traceback
			traceback.print_exc(file=sys.stdout)
			pass
		
	def getNextRecordedData(self, record):
		if len(record.list) == 0:
			return (None, None)
		notfound = True
		pos = record.playbackposition
		while notfound:
			if record.list[pos].command in [MSG2_VEHICLE_DATA, MSG2_CHAT]:
				notfound=False
			pos += 1
			if pos >= len(record.list):
				pos = 0
		record.playbackposition = pos
		self.logger.debug("played back position %d" % pos)
		packet = record.list[pos]
		
		nextpos = pos+1
		t = 0.2
		if nextpos >= len(record.list):
			nextpos = 0
			t = 2
			
		nextpacket = record.list[nextpos]
		if nextpos < len(record.list):
			t = nextpacket.time - packet.time
		
		if t > 1 or t < 0: 
			t = 0.2
		packet.source = 0
		return (packet, t)

	def sendMsg(self, packet):
		self.logger.debug("SEND| %-16s, source %d, destination %d, size %d, data-len: %d" % (commandNames[packet.command], packet.source, self.uid, packet.size, len(str(packet.data))))
		if packet.size == 0:
			# just header
			data = struct.pack('III', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('III'+str(packet.size)+'s', packet.command, packet.source, packet.size, str(packet.data))
		self.sendRaw(data)

	def receiveMsg(self):
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

		#if not command in [MSG2_VEHICLE_DATA, MSG2_CHAT]:
		self.logger.debug("RECV| %-16s, source %d, size %d, data-len: %d -- %s" % (commandNames[command], self.uid, size, len(content), note))
		return DataPacket(command, self.uid, size, content)

if __name__ == '__main__':
	ip = sys.argv[1]
	port = int(sys.argv[2])
	num = int(sys.argv[3])
	threads = []
	restarts = {}
	for i in range(num):
		threads.append(Client(ip, port, i, 0))
		restarts[i] = 0
	
	while restartClient:
		for i in range(num):
			if not threads[i].isAlive():
				restarts[i]+=1
				print "thread %d dead, restarting" % i
				threads[i] = Client(ip, port, i, restarts[i])
				threads[i].start()
				
			#else:
			#	print "thread %d alive!" % i
