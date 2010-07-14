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
restartCommands = []
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
	nickname=""

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

class Connector(threading.Thread):
	uid = 0
	overlays = {}
	clientInfo = {}
	connected=False
	
	loglines = []
	
	class logHandler(logging.Handler):
		def __init__(self, _parent):
			logging.Handler.__init__(self)
			self.parent = _parent
			
		def emit(self, record):
			msg = self.format(record)
			#self.callback(msg)
			self.parent.loglines.append(msg)
			self.flush()

			
	def getLog(self):
		return self.logger
	
	def __init__(self, ip, port, cid, restartnum, commands):
		self.ip = ip
		self.port = port
		self.cid = cid
		self.restartnum = restartnum
		loggername = 'client-%02d' % cid
		self.logger = logging.getLogger(loggername)
		
		
		lh = self.logHandler(self)
		lh.setLevel(logging.DEBUG)
		formatter = logging.Formatter('%(message)s')
		lh.setFormatter(formatter)
		self.logger.addHandler(lh)

		#self.logger.debug('__init__')
		self.runCond = True
		self.startupCommands = commands
		self.username = ''
		self.socket = None
		self.playback = None
			
		self.mode = MODE_NORMAL
		
		threading.Thread.__init__(self)

	
	def processCommand(self, cmd, packet=None, startup=False):
		global restartClient, restartCommands, eventStopThread
		try:
			self.logger.debug("CHAT| %s: %s" % (self.clientInfo[packet.source].nickname, packet.data))
		except:
			pass
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
		
		elif cmd == "!ping":
			self.sendChat("pong!")
		
		else:
			if cmd[0] == "!":
				self.logger.debug('command not found: %s' % cmd)

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
		if packet is None:
			return

		if packet.command != MSG2_VERSION:
			self.logger.debug('invalid handshake: MSG2_VERSION')
			self.disconnect()
			return
		version = packet.data
		
		packet = self.receiveMsg()
		if packet is None:
			return
		if packet.command != MSG2_TERRAIN_RESP:
			self.logger.debug('invalid handshake: MSG2_TERRAIN_RESP')
			self.disconnect()
			return

		self.terrain = packet.data
		
		data = struct.pack('20s40s40s', self.username, self.password, self.uniqueid)
		self.sendMsg(DataPacket(MSG2_USER_CREDENTIALS, 0, len(data), data))

		packet = self.receiveMsg()
		if packet is None:
			return
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
				if not packet is None:
					# record the used vehicles
					if packet.command == MSG2_USE_VEHICLE:
						data = str(packet.data).split('\0')
						self.clientInfo[packet.source] = ClientInfo()
						self.clientInfo[packet.source].truck = data[0]
						self.clientInfo[packet.source].nickname = data[1]
						
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
				self.disconnect()
				return None
			
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

def gui():
	import wx
	import os
	ID_ABOUT=101
	ID_OPEN=102
	ID_BUTTON1=110
	ID_EXIT=200
	ID_TIMER=201

	class ServerAdressWindow(wx.Dialog):
		ID_LIST=300
		ID_ADDSERVER=301
		ID_DELSERVER=302
		ID_CONSERVER=303
		ID_CANCEL=304
		servers = None
		def __init__(self,parent,id,title):
			wx.Dialog.__init__(self,parent,wx.ID_ANY, title)
			sizer = wx.BoxSizer(wx.HORIZONTAL)

			#list
			self.list = wx.ListCtrl(self, self.ID_LIST, wx.DefaultPosition, wx.DefaultSize) #, wx.LC_SMALL_ICON)
			wx.EVT_LIST_ITEM_ACTIVATED(self, self.ID_LIST, self.OnConnServer)
			sizer.Add(self.list, 1, wx.EXPAND)

			# buttons
			sizer2 = wx.BoxSizer(wx.VERTICAL)
			btn = wx.Button(self, self.ID_ADDSERVER, "&Add Server")
			wx.EVT_BUTTON(self, self.ID_ADDSERVER, self.OnAddServer)
			sizer2.Add(btn, 0, wx.EXPAND)
			
			btn2 = wx.Button(self, self.ID_DELSERVER, "&Delete Server")
			wx.EVT_BUTTON(self, self.ID_DELSERVER, self.OnDelServer)
			sizer2.Add(btn2, 0, wx.EXPAND)

			btn2 = wx.Button(self, self.ID_CONSERVER, "&Connect to server")
			wx.EVT_BUTTON(self, self.ID_CONSERVER, self.OnConnServer)
			sizer2.Add(btn2, 1, wx.EXPAND)

			btn3 = wx.Button(self, self.ID_CANCEL, "Ca&ncel")
			wx.EVT_BUTTON(self, self.ID_CANCEL, self.OnCancel)
			sizer2.Add(btn3, 0, wx.EXPAND)
			
			sizer.Add(sizer2, 0, wx.EXPAND)
			self.SetSizer(sizer)
			
			self.loadServers()
			self.updateList()
		
		def OnCancel(self, e):
			self.EndModal(wx.ID_CANCEL)
			
		def OnConnServer(self, e):
			if self.list.GetSelectedItemCount() != 1:
				return
			itemno = self.list.GetFirstSelected()
			if itemno < 0:
				return
			dataid = self.list.GetItemData(itemno)
			self.result = self.servers[dataid]
			self.EndModal(wx.ID_OK)
		
		def OnDelServer(self, e):
			if self.list.GetSelectedItemCount() != 1:
				return
			itemno = self.list.GetFirstSelected()
			if itemno < 0:
				return
			dataid = self.list.GetItemData(itemno)
			del self.servers[dataid]
			self.saveServers()
			self.updateList()

		def OnAddServer(self, e):
			ted = wx.TextEntryDialog(self, "Server Address", "Please enter the server address", "127.0.0.1", wx.OK)
			ted.ShowModal()
			addr = ted.GetValue()
			
			ted3 = wx.TextEntryDialog(self, "Server Port", "Please enter the server port", "12000", wx.OK)
			ted3.ShowModal()
			port = ted3.GetValue()
			
			ted2 = wx.TextEntryDialog(self, "Server Name", "Please enter a server name", "my server", wx.OK)
			ted2.ShowModal()
			name = ted2.GetValue()
			self.servers.append([addr, port, name])
			self.saveServers()
			self.updateList()

		def updateList(self):
			self.list.ClearAll()
			for i in range(0, len(self.servers)):
				try:
					server = self.servers[i]
					li = wx.ListItem()
					li.SetText(server[0]+":"+server[1]+"\n"+server[2])
					li.SetData(i)
					self.list.InsertItem(li)
				except:
					pass
			
		def loadServers(self):
			self.servers = []
			try:
				f = open('servers.txt', 'r')
				lines = f.readlines()
				f.close()
			except:
				return
			for line in lines:
				if line.strip() == '':
					continue
				self.servers.append(line.strip().split(';'))
			
		def saveServers(self):
			lines = []
			try:
				for server in self.servers:
					lines.append(';'.join(server)+'\n')
				f = open('servers.txt', 'w')
				f.writelines(lines)
				f.close()
			except:
				return
	
	class ServerPage(wx.Panel):
		ID_TXT=100
		connector=None
		def __init__(self, parent, connID, name, ip, port):
			wx.Panel.__init__(self, parent)
			self.connID = connID
			self.name = name
			self.ip = ip
			self.port = port
			#t = wx.StaticText(self, -1, "This is a PageOne object %s" % ip, (20,20))
			sizer = wx.BoxSizer(wx.HORIZONTAL)

			#left side
			panel_left = wx.Panel(self)
			sizer_left = wx.BoxSizer(wx.VERTICAL)
			self.text = wx.TextCtrl(panel_left, self.ID_TXT, "", wx.DefaultPosition, wx.DefaultSize, wx.TE_MULTILINE ^ wx.TE_READONLY ^ wx.SUNKEN_BORDER ^ wx.TE_RICH ^ wx.HSCROLL)
			font = wx.Font(10, wx.FONTFAMILY_TELETYPE, wx.FONTSTYLE_NORMAL, wx.FONTWEIGHT_NORMAL, False, "fixedsys", wx.FONTENCODING_SYSTEM)
			textattrib = wx.TextAttr(wx.BLACK, wx.NullColour, font, wx.TEXT_ALIGNMENT_DEFAULT)
			self.text.SetDefaultStyle(textattrib)

			sizer_left.Add(self.text, 1, wx.EXPAND)

			self.texti = wx.TextCtrl(panel_left, self.ID_TXT, "", wx.DefaultPosition, wx.DefaultSize)
			sizer_left.Add(self.texti, 0, wx.EXPAND)
			panel_left.SetSizer(sizer_left)
	
			sizer.Add(panel_left, 1, wx.EXPAND)
			
			#right side
			sizer_right = wx.BoxSizer(wx.VERTICAL)
			sizer_right.SetMinSize((150,100))
			panel_right = wx.Panel(self)
			
			box = wx.StaticBox(panel_right, -1, "Server Statistics")
			box_sizer = wx.StaticBoxSizer(box, wx.VERTICAL)	
			
			self.servermode = wx.StaticText(panel_right, -1, "Mode: Internet")
			box_sizer.Add(self.servermode, 1, wx.EXPAND)
			
			self.serveruptime = wx.StaticText(panel_right, -1, "Uptime: 0")
			box_sizer.Add(self.serveruptime, 1, wx.EXPAND)

			sizer_right.Add(box_sizer, 0, wx.EXPAND)
			
			panel_right.SetSizer(sizer_right)
			sizer.Add(panel_right, 0, wx.EXPAND)
			
			self.SetSizer(sizer)

			# main timer
			self.timer = wx.Timer(self, ID_TIMER)
			self.timer.Start(500)
			wx.EVT_TIMER(self, ID_TIMER, self.OnTimer)

			# connect
			self.connect()
		
		def connect(self):
			self.connector = Connector(self.ip, int(self.port), self.connID, 0, [])
			#self.connector.getLog().addHandler(self.logtext)
			self.connector.start()
	    
		def OnTimer(self,e):
			pass
			if not self.connector is None:
				#print self.connector.loglines
				msg = ""
				if len(self.connector.loglines) > 0:
					for line in self.connector.loglines:
						msg+=line.strip()+"\n"
					self.text.AppendText(msg)
					self.connector.loglines = []
			
	class MainWindow(wx.Frame):
		connCounter = 0
		def __init__(self,parent,id,title):
			wx.Frame.__init__(self,parent,wx.ID_ANY, title)
			self.SetSize((500,300))
			self.SetMinSize((500,300))

			self.CreateStatusBar() # A Statusbar in the bottom of the window
			
			# Setting up the menu.
			filemenu= wx.Menu()
			filemenu.Append(ID_OPEN, "&New Connection"," Connect to a server")
			filemenu.AppendSeparator()
			filemenu.Append(ID_ABOUT, "&About"," Information about this program")
			filemenu.AppendSeparator()
			filemenu.Append(ID_EXIT,"E&xit"," Terminate the program")

			# Creating the menubar.
			menuBar = wx.MenuBar()
			menuBar.Append(filemenu,"&Connection") # Adding the "filemenu" to the MenuBar
			self.SetMenuBar(menuBar)  # Adding the MenuBar to the Frame content.
			#wx.EVT_MENU(self, ID_ABOUT, self.OnAbout)
			wx.EVT_MENU(self, ID_EXIT, self.OnExit)
			wx.EVT_MENU(self, ID_OPEN, self.OnOpen)


			# Here we create a panel and a notebook on the panel
			p = wx.Panel(self)
			self.nb = wx.Notebook(p)
			
			# create the page windows as children of the notebook
			sizer = wx.BoxSizer()
			sizer.Add(self.nb, 1, wx.EXPAND)
			p.SetSizer(sizer)
			
		def OnExit(self,e):
			sys.exit(0)

		def OnOpen(self,e):
			serverSelectWindow = ServerAdressWindow(None, -1, "Select a server")
			if serverSelectWindow.ShowModal() == wx.ID_OK:
				server = serverSelectWindow.result
				self.connCounter += 1
				sp = ServerPage(self.nb, self.connCounter, server[2], server[0], server[1])
				self.nb.AddPage(sp, server[2]+' ('+server[0]+':'+server[1]+')')
			serverSelectWindow.Destroy()


	app = wx.PySimpleApp()
	frame = MainWindow(None, -1, "RoR Dedicated Server remote console")
	frame.Show()
	app.MainLoop()

if __name__ == '__main__':
	try:
		import psyco
		psyco.full()
	except ImportError:
		pass	
	gui()
