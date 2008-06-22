#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
import sys, struct, logging, threading, socket, random, time, string, os, os.path, time, math, copy, hashlib, math

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
MSG2_GAME_CMD = 1028
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
	1025:"RCON_COMMAND",
	1026:"RCON_COMMAND_FAILED",
	1027:"RCON_COMMAND_SUCCESS",
	1028:"GAME_CMD",
	
}

RORNET_VERSION = "RoRnet_2.1"
 
restartClient = False # this enabled auto restart of crashed bots
restartCommands = ['!connect'] # important ;)
eventStopThread = None

VERBOSELOG = False

MODE_NORMAL = 0

class Vector3:
	x=0
	y=0
	z=0
	def __init__(self, _x, _y, _z): 
		self.x = _x
		self.y = _y
		self.z = _z

	def length(self):
		return math.sqrt(self.x*self.x + self.y*self.y + self.z*self.z);

	def distanceTo(self, v):
		return Vector3(self.x-v.x, self.y-v.y, self.z-v.z).length()

	def __str__(self):
		return "%.1f,_%.1f,_%.1f" % (self.x, self.y, self.z) # _ to be on the safe side
 
class ClientInfo:
	time=0
	engine_speed=0
	engine_force=0
	flagmask=0
	position=None
	truck=None

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

class MissionManager:
	missions = []
	parentClient = None
	
	def __init__(self, clientClass):
		parentClient = clientClass
		# find all mission files
		import glob
		sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "missions"))
		files = glob.glob('missions/*.py')
		#print files
		for file in files:
			print "adding mission "+os.path.basename(file)
			mission = __import__(os.path.basename(file).rstrip(".py"))
			#mission.MAINCLASS.startForClient(0)
			self.missions.append(mission)
		
	def newClient(self, clientinfo):
		print "got new client!"
		 
		#find valid missions
		avMissions = []
		for mission in self.missions:
			if (clientinfo.truck in mission.CONFIG['fortruck'] or 'all' in mission.CONFIG['fortruck']) and \
			   (parentClient.terrain in mission.CONFIG['forterrain'] or 'all' in mission.CONFIG['forterrain']):
				avMissions.append(mission)
		
		
		
class Client(threading.Thread):
	uid = 0
	overlays = {}
	clientInfo = {}
	connected=False
	
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
		
		self.missionManager = MissionManager(self)
		
		self.mode = MODE_NORMAL
		
		threading.Thread.__init__(self)

	
	def processCommand(self, cmd, packet=None, startup=False):
		global restartClient, restartCommands, eventStopThread
		self.logger.debug("%s / %d" % (cmd, self.mode))
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
		
		elif cmd == "!overlaytest":
			f=open("testOverlay.overlay", "r")
			overlay = f.read()
			f.close()
			
			self.sendRCon("createoverlay %04d "%(packet.source)+overlay)			
			self.sendRCon("setoverlayvisible %d, 1, tracks/MP/testOverlay" % (packet.source))
			self.sendRCon("setoverlayelementcolor %d, 1,0,0,1, tracks/MP/testOverlay/text1" % (packet.source))
			self.sendRCon("setoverlayelementtext %d %d tracks/MP/testOverlay/text1" % (packet.source, time.time()))

		elif cmd == "!showStats":
			oname = "gameOverlay.overlay"
			f=open(oname, "r")
			overlay = f.read()
			f.close()
			if not oname in self.overlays.keys():
				self.overlays[oname] = {}
			if not packet.source in self.overlays[oname].keys():
				# load overlay for that client
				self.overlays[oname][packet.source] = True
				self.sendRCon("createoverlay %04d "%(packet.source)+overlay)			
				self.sendRCon("setoverlayelementcolor %d, 1,1,1,1, tracks/MP/gameoverlay/text1" % (packet.source))
				self.sendRCon("setoverlayelementcolor %d, 1,1,1,1, tracks/MP/gameoverlay/text2" % (packet.source))
				self.sendRCon("setoverlayelementtext %d _ tracks/MP/gameoverlay/text1" % (packet.source))
				self.sendRCon("setoverlayelementtext %d _ tracks/MP/gameoverlay/text2" % (packet.source))
			
			self.sendRCon("setoverlayvisible %d, 1, tracks/MP/gameoverlay" % (packet.source))
			self.updateStats(packet.source)
			
		elif cmd == "!hideStats":
			oname = "gameOverlay.overlay"
			if packet.source in self.overlays[oname].keys() and self.overlays[oname][packet.source] == True:
				self.sendRCon("setoverlayvisible %d, 0, tracks/MP/gameoverlay" % (packet.source))
		
		else:
			if cmd[0] == "!":
				self.logger.debug('command not found: %s' % cmd)

	def updateStats(self, uid):
		#self.logger.debug('updateStats()')
		
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text1", "You_are_Player_number_%d" % (uid))
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text2", time.ctime().replace(" ", "_"))
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text3", "Your_position:_%s" % (self.clientInfo[uid].position.__str__()))
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text4", "Playing_since:_%d_seconds" % (self.clientInfo[uid].time/1000))
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text5", "Your_RPM:_%d" % (self.clientInfo[uid].engine_speed))
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text6", ".")
		self.updateClientOverlayText(uid, "tracks/MP/gameoverlay/text7", ".")
	
	def updateClientOverlayText(self, uid, oname, text):
		self.sendRCon("setoverlayelementtext %d %s %s" % (uid, text, oname))
		
	def disconnect(self):
		if not self.socket is None:
			self.sendMsg(DataPacket(MSG2_DELETE, 0, 0, 0))
			self.logger.debug('closing socket')
			self.socket.close()
		self.socket = None
		self.connected=False
	
	def connect(self):
		self.logger.debug('creating socket')
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.logger.debug('connecting to server')
		try:
			self.socket.connect((self.ip, self.port))
		except Exception, e:
			self.logger.debug('connect error: '+str(e))
			return False

		self.socket.settimeout(60)

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
		if packet.command == MSG2_FULL:
			self.logger.debug('server is full!')
			self.disconnect()
			return
		
		if packet.command != MSG2_WELCOME:
			self.logger.debug('invalid handshake: MSG2_WELCOME')
			self.disconnect()
			return
		
		self.connected=True
		self.sendMsg(DataPacket(MSG2_USE_VEHICLE, 0, len(self.truckname), self.truckname))
		
		data = struct.pack('I', self.buffersize)
		self.sendMsg(DataPacket(MSG2_BUFFER_SIZE, 0, len(data), data))
					
	def run(self):
		# get bot's uid
		s = hashlib.sha1()
		s.update("GameBot")
		#print s.hexdigest()
	
		# some default values
		self.uniqueid = s.hexdigest()
		self.password = ""
		self.username = "GameBot"
		self.buffersize = 1 # 'data' - String
		self.truckname = "spectator"
		
		# dummy data to prevent timeouts
		dummydata = self.packPacket(DataPacket(MSG2_VEHICLE_DATA, 0, 1, "1"))

		lasttime = time.time()
		while self.runCond:
			if self.connected:		
				if len(self.startupCommands) > 0:
					cmd = self.startupCommands.pop(0).strip()
					if cmd != "":
						self.logger.debug('executing startup command %s' % cmd)
						self.processCommand(cmd, None, True)
					repeat = (len(startupCommands) > 0)
			
				packet = self.receiveMsg()
				if packet == False:
					self.logger.debug('server closed the connection')
					self.disconnect()
				if not packet is None:
					# record the used vehicles
					if packet.command == MSG2_USE_VEHICLE:
						data = str(packet.data).split('\0')
						self.clientInfo[packet.source] = ClientInfo()
						self.clientInfo[packet.source].truck = data
						print data
						
					if packet.command == MSG2_VEHICLE_DATA:
						ci = self.unPackClientInfo(packet.data)
						self.clientInfo[packet.source].time = ci.time
						self.clientInfo[packet.source].engine_speed = ci.engine_speed
						self.clientInfo[packet.source].engine_force = ci.engine_force
						self.clientInfo[packet.source].flagmask = ci.flagmask						
						self.clientInfo[packet.source].position = ci.position						
									
					if packet.command == MSG2_CHAT:
						self.processCommand(str(packet.data), packet)
			
					if  time.time() - lasttime > 1:
						oname = "gameOverlay.overlay"
						if oname in self.overlays.keys() and packet.source in self.overlays[oname].keys() and self.overlays[oname][packet.source] == True:
							self.updateStats(packet.source)
						lasttime = time.time()
			
				if self.mode == MODE_NORMAL:
					# to prevent timeouts in normal mode
					if not self.sendRaw(dummydata):
						self.logger.debug('server closed the connection')
						self.disconnect()
			else:
				# retry connection
				if time.time() - lasttime > 10:
					self.logger.debug('retrying to connect...')
					self.connect()
					lasttime = time.time()
				
	def sendChat(self, msg, uid=-1):
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
		
	def sendRCon(self, command):
		self.sendMsg(DataPacket(MSG2_RCON_COMMAND, 0, len(command), command))
		
	def sendRaw(self, data):
		if self.socket is None:
			self.logger.debug('cannot send: not connected!')
			self.connceted = False
			return False
		try:
			self.socket.send(data)
			return True
		except Exception, e:
			self.logger.debug('sendMsg error: '+str(e))
			import traceback
			traceback.print_exc(file=sys.stdout)
			return False

	def packPacket(self, packet):
		if packet.size == 0:
			# just header
			data = struct.pack('III', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('III'+str(packet.size)+'s', packet.command, packet.source, packet.size, str(packet.data))
		return data
		
	def unPackVector(self, data):
		size = struct.calcsize('fff')
		#print size
		args = struct.unpack('fff', data[0:size])
		#print args
		return Vector3(args[0], args[1], args[2])

	def unPackClientInfo(self, data):
		format = 'iffIfff'
		size = struct.calcsize(format)
		#print size
		args = struct.unpack(format, data[0:size])
		#print args
		ci = ClientInfo()
		ci.time = args[0]
		ci.engine_speed = args[1]
		ci.engine_force = args[2]
		ci.flagmask = args[3]
		ci.position = Vector3(args[4], args[5], args[6])
		return ci
		
	def sendMsg(self, packet):
		if self.socket is None:
			return
		if VERBOSELOG:
			self.logger.debug("SEND| %-16s, source %d, destination %d, size %d, data-len: %d" % (commandNames[packet.command], packet.source, self.uid, packet.size, len(str(packet.data))))
		self.sendRaw(self.packPacket(packet))

	def receiveMsg(self):
		if self.socket is None:
			return None
		note = ""
		headersize = struct.calcsize('III')
		data = ""
		readcounter = 0
		while len(data) < headersize:
			try:
				data += self.socket.recv(headersize-len(data))
			except Exception, e:
				self.logger.debug('receiveMsg error: '+str(e))
				return False
			
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
	num = 1
	startupCommands = ['!connect', '!say hello!']
	if len(sys.argv) == 5:
		startupCommands = sys.argv[4].split(';')
	
	# Import Psyco if available
	#try:
	#	import psyco
	#	psyco.full()
	#except ImportError:
	#	pass	
	
	threads = []
	restarts = {}
	lastrestart = {}
	try:
		# try for keyboard interrupt
		for i in range(num):
			threads.append(Client(ip, port, i, 0, copy.copy(startupCommands)))
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
					threads[i] = Client(ip, port, i, restarts[i], rcmd)
					threads[i].start()
					lastrestart[i] = time.time()
					
				#else:
				#	print "thread %d alive!" % i
		print "exiting peacefully"
	except KeyboardInterrupt:
		print "exiting ..."
		sys.exit(0)
