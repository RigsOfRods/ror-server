#!/bin/env python
# made by thomas in 5 hours - no guarantees ;)
import sys, struct, logging, threading, SocketServer, time

SERVERNAME = sys.argv[1]
TERRAIN = sys.argv[2]
PORT = int(sys.argv[3])
#SERVERNAME = "testme"
#TERRAIN = 'nhelens'
#PORT = 12000
IP = '78.47.152.220'

MOTD = """^1 hi ^2and welcome to ^3our ^4little ^5server!"""

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

APISERVER = "api.rigsofrods.com"

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
	1028:"GAME_CMD"
}

RORNET_VERSION = "RoRnet_2.1"

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

class Client:
	def __init__(self,username, vehicle, handler, uid):
		self.username = username
		self.vehicle = vehicle
		self.handler = handler
		self.uid = uid
		self.flow = True
	def __str__(self):
		return "%d - %s - %s" % (self.uid, self.username, self.vehicle)
		

class Sequencer:
	clients = {}
	uidcounter=0
	
	def __init__(self):
		self.logger = logging.getLogger('sequencer')
		self.logger.debug('__init__')
		self.lock = threading.Lock()
		return
		
	def addClient(self, username, vehicle, handler):
		self.logger.debug('addClient')
		cuid = self.uidcounter
		self.lock.acquire() 
		self.clients[cuid] = Client(username, vehicle, handler, cuid)
		self.lock.release()
		
		self.logger.debug('clients: %s' % (self.clients))
		
		# push the client to all existing clients
		self.logger.debug('announcing new client to all other clients')
		self.sendClientList()		

		self.uidcounter+=1
		return cuid
	
	def sendClientList(self):
		self.logger.debug('sendInitialClientList')
		self.lock.acquire()
		for uid_a in self.clients.keys():
			client_a = self.clients[uid_a]
			for uid_b in self.clients.keys():
				if uid_b == uid_a:
					# dont register self
					continue
				client_b = self.clients[uid_b]
				client_a.handler.queueMessage(DataPacket(MSG2_USE_VEHICLE, uid_b, len(client_b.vehicle)+len(client_b.username), client_b.vehicle+"\0"+client_b.username))
				#self.logger.debug(str(uid_a)+" -> "+str(uid_b))
		self.lock.release()

	def delClient(self, uid):
		self.logger.debug('delClient')
		self.lock.acquire()
		deleted = False
		if uid in self.clients.keys():
			del self.clients[uid]
			deleted = True
		self.lock.release()

		if deleted:
			self.broadcastData(DataPacket(MSG2_DELETE, uid, 0, 0))
	
	def getNumClients(self):
		self.lock.acquire()
		num = len(self.clients)
		self.lock.release()
		return num

		if deleted:
			self.broadcastData(DataPacket(MSG2_DELETE, uid, 0, 0))
		
	def broadcastData(self, packet, destination=-1):
		""" 
			-2 = send to all, including self
			-1 = send to all, excluding self
		"""
		#self.logger.debug('broadcastData')
		self.lock.acquire()

		data = None
		if packet.size == 0:
			# just header
			data = struct.pack('IiI', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('IiI'+str(packet.size)+'s', packet.command, packet.source, packet.size, packet.data)
		
		self.logger.debug("BRDC| %-16s, source %d, size %d, data-len: %d" % (commandNames[packet.command], packet.source, packet.size, len(str(packet.data))))
		if destination < 0:
			for kuid in self.clients.keys():
				client = self.clients[kuid]
				if kuid == packet.source and destination == -1:
					continue;
				if client.flow:
					client.handler.sendMsg_fast(data)
		#else:
			#client = self.clients[kuid]
			#if client.flow:
			#	client.handler.queueMessage(packet)
		self.lock.release()
		

SEQUENCER = Sequencer()

class Notifier(threading.Thread):
	def run(self):
		import urllib, urllib2, time
		self.logger = logging.getLogger('notifier')
		self.logger.debug('started')
		url = "http://%s/register/?name=%s&description=%s&ip=%s&port=%i&terrainname=%s&maxclients=%i&version=%s&pw=%d&rcon=%d" % (APISERVER, SERVERNAME, "", IP, PORT, TERRAIN, 16, RORNET_VERSION, 0, 0)
		self.logger.debug('registering at master server ...')
		f = urllib2.urlopen(url)
		res = f.read()
		f.close()
		if res.find("error") == -1:
			id = res
		else:
			self.logger.debug('register error: %s' % res)
			return
		
		while True:
			clientnum = SEQUENCER.getNumClients()
			url = "http://%s/heartbeat/?challenge=%s&currentusers=%d" % (APISERVER, id, clientnum)
			f = urllib2.urlopen(url)
			result = f.read()
			f.close()
			if result == 'ok':
				self.logger.debug('heartbeat ok')
			else:
				self.logger.debug('heartbeat failed: %s'%result)
			time.sleep(60)

import BaseHTTPServer, urlparse
class MyWebRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
	def do_GET(self):
		if self.path == '/':
			parsed_path = urlparse.urlparse(self.path)
			message = '\n'.join([
				'CLIENT VALUES:',
				'client_address=%s (%s)' % (self.client_address,
											self.address_string()),
				'command=%s' % self.command,
				'path=%s' % self.path,
				'real path=%s' % parsed_path.path,
				'query=%s' % parsed_path.query,
				'request_version=%s' % self.request_version,
				'',
				'SERVER VALUES:',
				'server_version=%s' % self.server_version,
				'sys_version=%s' % self.sys_version,
				'protocol_version=%s' % self.protocol_version,
				'',
				])
			msg = "Hello World!"
			self.send_response(200)
			self.end_headers()
			self.wfile.write(message)
		else: 
			self.send_error(404)

class MyWebserver(threading.Thread):
	def run(self):
		self.logger = logging.getLogger('webserver')
		self.logger.debug('webserver on port %d started!'% (PORT+1))
		server = BaseHTTPServer.HTTPServer(('',PORT+1), MyWebRequestHandler)
		server.serve_forever()


class RoRHandler(SocketServer.StreamRequestHandler):
	uid = 0
	def __init__(self, request, client_address, server):
		self.logger = logging.getLogger('handler')
		self.logger.debug('__init__')
		self.readlock = threading.Lock()
		self.writelock = threading.Lock()
		SocketServer.BaseRequestHandler.__init__(self, request, client_address, server)
		return

	def setup(self):
		self.logger.debug('setup')
		return SocketServer.StreamRequestHandler.setup(self)
	
	def finish(self):
		self.logger.debug('finish')
		return SocketServer.StreamRequestHandler.finish(self)
		
	def sendMsg(self, packet):
		#if packet.command != MSG2_VEHICLE_DATA:
		self.logger.debug("SEND| %-16s, source %d, destination %d, size %d, data-len: %d" % (commandNames[packet.command], packet.source, self.uid, packet.size, len(str(packet.data))))
		if packet.size == 0:
			# just header
			data = struct.pack('IiI', packet.command, packet.source, packet.size)
		else:
			data = struct.pack('IiI'+str(packet.size)+'s', packet.command, packet.source, packet.size, packet.data)
		try:
			self.writelock.acquire()
			self.wfile.write(data)
			self.writelock.release()
		except Exception, e:
			self.logger.debug('sendMsg_fast error: '+str(e))
			import traceback
			traceback.print_exc(file=sys.stdout)
			pass

	def sendMsg_fast(self, data):
		try:
			self.writelock.acquire()
			self.wfile.write(data)
			self.writelock.release()
		except Exception, e:
			self.logger.debug('sendMsg_fast error: '+str(e))
			import traceback
			traceback.print_exc(file=sys.stdout)
			pass
			
	def receiveMsg(self):
		note = ""
		headersize = struct.calcsize('IiI')
		data = ""
		readcounter = 0
		while len(data) < headersize:
			data += self.rfile.read(headersize-len(data))
		if readcounter > 1:
			note += "HEADER SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		(command, source, size) = struct.unpack('IiI', data)
		
		data = ""
		readcounter = 0
		while len(data) < size:
			readcounter+=1
			data += self.rfile.read(size-len(data))
		if readcounter > 1:
			note += "DATA SEGMENTED INTO %s SEGMENTS!" % readcounter
		
		content = struct.unpack(str(size) + 's', data)
		content = content[0]

		#if not command in [MSG2_VEHICLE_DATA, MSG2_CHAT]:
		self.logger.debug("RECV| %-16s, source %d, size %d, data-len: %d -- %s" % (commandNames[command], self.uid, size, len(content), note))
		return DataPacket(command, self.uid, size, content)
	
	def queueMessage(self, packet):
		self.sendMsg(packet)
		
	def receiveLoop(self):
		run=True
		while run:
			try:
				packet = self.receiveMsg()
				
				overlaytest4 = "setoverlayelementtext "+str(time.time())+" tracks/MP/testOverlay/text1"
				self.sendMsg(DataPacket(MSG2_GAME_CMD, -1, len(overlaytest4), overlaytest4))
				
				if packet.command == MSG2_VEHICLE_DATA:
					SEQUENCER.broadcastData(packet)
				elif packet.command == MSG2_DELETE:
					msg = "%s disconnected on request" % self.username
					SEQUENCER.broadcastData(DataPacket(MSG2_CHAT, 0, len(msg), msg))
					self.logger.debug('exiting on request')
					SEQUENCER.delClient(self.uid)
					run=False
				elif packet.command == MSG2_CHAT:
					SEQUENCER.broadcastData(packet)
					self.logger.debug("CHAT | "+self.username+": "+str(packet.data))
			except Exception, e:
				self.logger.debug('error, disconnecting: '+str(e))
				import traceback
				traceback.print_exc(file=sys.stdout)
				SEQUENCER.delClient(self.uid)
				run=False
				pass
	
	def handle(self):
		self.logger.debug('handle')
		
		packet = self.receiveMsg()
		if packet.command == MSG2_HELLO:
			self.sendMsg(DataPacket(MSG2_VERSION, 0, len(RORNET_VERSION), RORNET_VERSION))
			self.sendMsg(DataPacket(MSG2_TERRAIN_RESP, 0, len(TERRAIN), TERRAIN))
			packet = self.receiveMsg()
			if packet.command == MSG2_USER_CREDENTIALS:
				(username, password, uniqueid) = struct.unpack('20s40s40s', packet.data)
				self.logger.debug("new client: %s, %s, %s" % (username.strip(), password.strip(), uniqueid.strip()))
				self.username = username.strip()
				self.password = password.strip()
				self.uniqueid = uniqueid.strip()
				self.sendMsg(DataPacket(MSG2_WELCOME, 0, 0, 0))
				# wait for vehicle info
				packet = self.receiveMsg()
				if packet.command == MSG2_USE_VEHICLE:
					vehicle = struct.unpack(str(packet.size)+'s', packet.data)
					self.vehicle = vehicle[0].strip()
					packet = self.receiveMsg()
					if packet.command == MSG2_BUFFER_SIZE:
						buffersize = struct.unpack('I', packet.data)
						self.buffersize = int(buffersize[0])
						# init successfully, neter the main loop :)
						self.uid = SEQUENCER.addClient(self.username, self.vehicle, self)
						
						self.sendMsg(DataPacket(MSG2_CHAT, -1, len(MOTD), MOTD))
						
						gametest = "newgoal 0,0,0, test123"
						self.sendMsg(DataPacket(MSG2_GAME_CMD, -1, len(gametest), gametest))

						f=open("testOverlay.overlay", "r")
						overlay = f.read()
						f.close()
						
						overlaytest = "createoverlay "+overlay
						print overlaytest
						self.sendMsg(DataPacket(MSG2_GAME_CMD, -1, len(overlaytest), overlaytest))
						
						overlaytest2 = "setoverlayvisible 1, tracks/MP/testOverlay"
						print overlaytest2
						self.sendMsg(DataPacket(MSG2_GAME_CMD, -1, len(overlaytest2), overlaytest2))

						overlaytest3 = "setoverlayelementcolor 1,0,0,1, tracks/MP/testOverlay/text1"
						print overlaytest3
						self.sendMsg(DataPacket(MSG2_GAME_CMD, -1, len(overlaytest3), overlaytest3))
						
						overlaytest4 = "setoverlayelementtext "+str(time.time())+" tracks/MP/testOverlay/text1"
						self.sendMsg(DataPacket(MSG2_GAME_CMD, -1, len(overlaytest4), overlaytest4))
						
						self.receiveLoop()
				
		return

class ThreadedRoRServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
	pass
	
	def close(self):
		sys.exit(0)

if __name__ == '__main__':
	import socket
	import threading

	# Import Psyco if available
	try:
		import psyco
		psyco.full()
	except ImportError:
		pass	
	
	address = ('0.0.0.0', PORT)
	server = ThreadedRoRServer(address, RoRHandler)
	ip, port = server.server_address # find out what port we were given
	print "running on %s:%d" % (ip, port)

	#t = threading.Thread(target=server.serve_forever)
	#t.setDaemon(True) # don't hang on exit
	#t.start()
	#print 'Server loop running in thread:', t.getName()

	Notifier().start()
	MyWebserver().start()
	
	try:
		server.serve_forever()
	except KeyboardInterrupt:
		server.close()
		server.socket.close()
		sys.exit(0)

