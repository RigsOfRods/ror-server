#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
import sys, struct, logging, threading, socket, random, time, string, os, os.path, time, math, copy
import signal

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

client = None
RORNET_VERSION = "RoRnet_2.1"
VERBOSELOG = False

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
	
class Client(threading.Thread):
	uid = 0
	oclients = {}
	
	def __init__(self, ip, port, name, restartnum, commands):
		self.ip = ip
		self.port = port
		self.logger = logging.getLogger()
		self.logger.debug('__init__')
		self.running = True # sighandler will change this value
		self.startupCommands = commands
		self.username = name
		self.socket = None
		
		# some default values
		self.uniqueid = "1337"
		self.password = ""
		self.buffersize = 1 # 'data' - String
		self.truckname = "spectator"
		
		self.lock = threading.Lock()
		
		threading.Thread.__init__(self)

	def processCommand(self, cmd, packet=None, startup=False):
		self.logger.debug("cmd: %s " % (cmd))

		if cmd[:4] == "!say":
			self.sendChat(cmd[4:].strip())
		#this is the exit condition
		elif cmd == "!shutdown":
			self.disconnect()
			return False
		elif cmd == "!ping":
			self.sendChat("gnip!")
		elif cmd[0] != "!":
			self.sendChat( "!"+cmd )
		return True
	
	def sendChat(self, msg):
		maxsize = 50
		msga = msg[: max( len(msg), maxsize )]
		self.sendMsg(DataPacket(MSG2_CHAT, 0, len(msga), msga))
		
	def disconnect(self):
		if not self.socket is None:
			self.sendMsg(DataPacket(MSG2_DELETE, 0, 0, 0))
			self.logger.debug('closing socket')
			self.socket.close()
		self.socket = None
		self.running = False
	
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
			return False
		version = packet.data
		
		packet = self.receiveMsg()
		if packet.command != MSG2_TERRAIN_RESP:
			self.logger.debug('invalid handshake: MSG2_TERRAIN_RESP')
			self.disconnect()
			return False
		self.terrain = packet.data
		
		data = struct.pack('20s40s40s', str(self.username), str(self.password),\
						   str(self.uniqueid) )
		self.sendMsg(DataPacket(MSG2_USER_CREDENTIALS, 0, len(data), data))

		packet = self.receiveMsg()
		if packet.command != MSG2_WELCOME:
			self.logger.debug('invalid handshake: MSG2_WELCOME')
			self.disconnect()
			return False
		
		self.sendMsg(DataPacket(MSG2_USE_VEHICLE, 0, len(self.truckname), self.truckname))
		
		data = struct.pack('I', self.buffersize)
		self.sendMsg(DataPacket(MSG2_BUFFER_SIZE, 0, len(data), data))
		self.logger.debug( "connect finished" )
					
	def run(self):
		
		# dummy data to prevent timeouts
		dummydata = self.packPacket(DataPacket(MSG2_VEHICLE_DATA, 0, 1, "1"))

		try:    
			while self.running:
				packet = self.receiveMsg()
				if not packet is None:
					# record the used vehicles
					if packet.command == MSG2_USE_VEHICLE:
						data = str(packet.data).split('\0')
						self.oclients[packet.source] = data
						
					elif packet.command == MSG2_CHAT:
						self.processCommand(str(packet.data), packet)
			
				self.sendRaw(dummydata)

		except Exception, e:
			print str(e)
			import traceback
			traceback.print_exc(file=sys.stdout)

			return
		
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

	def packPacket(self, packet):
		if packet.size == 0:
			# just header
			data = struct.pack('III', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('III'+str(packet.size)+'s', packet.command, packet.source, packet.size, str(packet.data))
		return data
		
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
			data += self.socket.recv(headersize-len(data))
		if readcounter > 1:
			note += "HEADER SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		(command, source, size) = struct.unpack('III', data)
		
		data = ""
		readcounter = 0
		while len(data) < size and self.running:
			readcounter+=1
			data += self.socket.recv(size-len(data))
		if readcounter > 1:
			note += "DATA SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		if not self.running:
			return None
		
		content = struct.unpack(str(size) + 's', data)
		content = content[0]

		if not command in [MSG2_VEHICLE_DATA, MSG2_CHAT]:
			self.logger.debug("RECV| %-16s, source %d, size %d, data-len: %d -- %s" % (commandNames[command], self.uid, size, len(content), note))
		return DataPacket(command, source, size, content)
		
def printhelp():
	print "Usage: python client.py <ip_address> <port> <client_name>"
	
def sighandler( signum, frame ):
	global client
	client.disconnect()

if __name__ == '__main__':
	
	# catch errors when we don't supply enough parameters  
	try:
		ip = sys.argv[1]
		port = int(sys.argv[2])
		name = str(sys.argv[3])
		startupCommands = ['!connect', '!say hello!']
		if len(sys.argv) == 5:
			startupCommands = sys.argv[4].split(';')
	except Exception, e:
		print str(e)
		import traceback
		traceback.print_exc(file=sys.stdout)

		printhelp()
		sys.exit(1)
		
	signal.signal( signal.SIGTERM, sighandler )
	
	client = Client(ip, port, name, 0, copy.copy(startupCommands))
	client.connect()
	client.run()
	client.disconnect()
	print "exiting ..."
		