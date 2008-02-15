#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
import sys, struct, logging, threading, socket, random, time, string

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
UCOUNTER = 0

class DataPacket:
	source=0
	command=0
	size=0
	data=0
	def __init__(self, command, source, size, data):
		self.source = source
		self.command = command
		self.size = size
		self.data = data

class Client(threading.Thread):
	uid = 0
	
	def __init__(self, ip, port, cid):
		self.ip = ip
		self.port = port
		self.cid = cid
		self.logger = logging.getLogger('client')
		self.logger.debug('__init__')
		threading.Thread.__init__(self)

		
	def run(self):
		self.logger.debug('creating socket')
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.logger.debug('connecting to server')
		
		self.socket.connect((self.ip, self.port))

		self.sendMsg(DataPacket(MSG2_HELLO, 0, len(RORNET_VERSION), RORNET_VERSION))
		packet = self.receiveMsg()
		if packet.command != MSG2_VERSION:
			logger.debug('invalid handshake: MSG2_VERSION')
			return
		version = packet.data
		
		packet = self.receiveMsg()
		if packet.command != MSG2_TERRAIN_RESP:
			logger.debug('invalid handshake: MSG2_VERSION')
			return
		
		terrain = packet.data
		username = "ATC_"+str(self.cid)
		password = ""
		uniqueid = "1337"
		buffersize = random.randint(50, 2000)
		
		data = struct.pack('20s40s40s', username, password, uniqueid)
		self.sendMsg(DataPacket(MSG2_USER_CREDENTIALS, 0, len(data), data))

		packet = self.receiveMsg()
		if packet.command != MSG2_WELCOME:
			self.logger.debug('invalid handshake: MSG2_WELCOME')
			return
		
		data = "_random_.truck"
		self.sendMsg(DataPacket(MSG2_USE_VEHICLE, 0, len(data), data))
		
		data = struct.pack('I', buffersize)
		self.sendMsg(DataPacket(MSG2_BUFFER_SIZE, 0, len(data), data))
	
		data = ""
		for i in range(buffersize):
			data += random.choice(string.letters + string.digits)

		counter = 0
		counter_max = random.randint(200, 500)
		run = True
		while run:
			#self.logger.debug('sending truck data %d' % (counter))
			self.sendMsg(DataPacket(MSG2_VEHICLE_DATA, 0, len(data), data))
			time.sleep(0.2)
			
			self.receiveMsg()
			
			counter += 1
			if counter_max > 0 and counter > counter_max:
				run=False
		
		self.logger.debug('closing socket')
		self.socket.close()
		

	def sendRaw(self, data):
		try:
			self.socket.send(data)
		except Exception, e:
			self.logger.debug('sendMsg error: '+str(e))
			import traceback
			traceback.print_exc(file=sys.stdout)
			pass
		
	def sendMsg(self, packet):
		self.logger.debug("SEND| %-16s, source %d, destination %d, size %d, data-len: %d" % (commandNames[packet.command], packet.source, self.uid, packet.size, len(str(packet.data))))
		if packet.size == 0:
			# just header
			data = struct.pack('III', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('III'+str(packet.size)+'s', packet.command, packet.source, packet.size, packet.data)
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
	for i in range(num):
		threads.append(Client(ip, port, i))
	
	while True:
		for i in range(num):
			if not threads[i].isAlive():
				print "thread %d dead!" % i
				threads[i] = Client(ip, port, i)
				threads[i].start()
			#else:
			#	print "thread %d alive!" % i
		
	
	
	
	


