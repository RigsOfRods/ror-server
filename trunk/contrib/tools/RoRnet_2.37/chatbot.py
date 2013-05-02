#!/bin/env python

from RoRnet_237.client import *

from Tkinter import *
import re

class chatbot(RoRClient):
	
	def __init__(self):
	
		# Set our information
		user                = user_info_t()
		user.username       = "services"
		user.usertoken      = "cifpo_moderator"
		user.serverpassword = "secret"
		user.language       = "nl_BE"
		user.clientname     = "chatbot"
		user.clientversion  = "1"
		
		# Set the server information
		serverinfo          = server_info_t()
		serverinfo.host     = "178.239.60.187"
		serverinfo.port     = 14006
		
		# Create the GUI
		self.gui = myGUI(callback=self.guiCallback)
		#self.messageHandler = self.gui.printSystemMessage
			
		# Original init method
		RoRClient.__init__(self, True)
		
		# Set the timer
		self.gui.master.after(200, self.processOnce)
		
		# and connect as well
		self.connect(user, serverinfo)
		self.gui.setStatus(user.username)
		self.gui.printJoinMessage("We joined the server as %s (%s - %d - %s %s)." % (user.username, user.language, user.uniqueID, user.clientname, user.clientversion))
		
		self.gui.mainloop()
		
		# Do something
		# self.server.kick(2, "testing!")
		
		# disconnect and exit
		# self.server.disconnect()
	
	def processOnce(self):
		RoRClient.processOnce(self, False)
		self.gui.master.after(200, self.processOnce)
	
	def on_publicChat(self, uid, message):
		self.gui.printPublicChatMessage(self.server.sm.getUsername(uid), message)

	def on_privateChat(self, uid, message):
		self.gui.printPrivateChatMessage(self.server.sm.getUsername(uid), message)

	def on_serverMessage(self, message):
		self.gui.printSystemMessage(message)
	
	def on_userJoin(self, uid, user):
		self.gui.printJoinMessage("User %s (%s) joined the server (%d - %s %s)." % (user.username, user.language, uid, user.clientname, user.clientversion))
		
	def on_userLeave(self, uid):
		self.gui.printLeaveMessage("User %s left the server." % (self.server.sm.getUsername(uid)))
		
	def on_streamRegister(self, uid, stream):
		self.gui.printStreamRegisterMessage("User %s spawned a new vehicle: %s" % (self.server.sm.getUsername(uid), stream.name))
		
	def guiCallback(self, cmd, args):
		if cmd=="chat":
			self.server.sendChat(args[0])
			self.gui.printPublicChatMessage(self.user.username, args[0])
		if cmd=="privchat":
			self.server.privChat(args[0], args[1])
			self.gui.printPrivateChatMessage(self.user.username, args[1])
		elif cmd=="disconnect":
			self.server.disconnect()
    

def dummyCallback(cmd, args):
	pass
	
class myGUI(Frame):              
	def __init__(self, master=None, callback=dummyCallback):
		self.sendCmd = callback
	
		# Create window
		Frame.__init__(self, master)
		self.grid(sticky=N+S+E+W)
		self.setStatus("Not Connected")
		self.master.geometry("650x300")
		
		# Create the message box
		self.messageFrame = Frame(self)
		self.messageFrame.grid(row=0, column=0, sticky=N+S+W+E)
		self.messageBoxScrollY = Scrollbar(self.messageFrame, orient=VERTICAL)
		self.messageBoxScrollY.grid(row=0, column=1, sticky=N+S)
		self.messageBox = Listbox(self.messageFrame, yscrollcommand=self.messageBoxScrollY.set, activestyle="none", bd="0p", highlightcolor="#FFFFFF", highlightthickness="0p", relief=FLAT, selectmode=SINGLE)
		self.messageBox.grid(row=0, column=0, sticky=N+S+E+W)
		self.messageBoxScrollY["command"] = self.messageBox.yview
		self.messageFrame.columnconfigure(0, weight=1)
		self.messageFrame.rowconfigure(0, weight=1)
				
		# Create the entry box
		self.entryFrame = Frame(self)
		self.entryFrame.grid(row=1, column=0, sticky=N+S+W+E)
		self.entryText = StringVar()
		self.entryBox = Entry(self.entryFrame, textvariable=self.entryText)
		self.entryBox.bind("<Return>", self.evSubmitMessage)
		self.entryBox.bind("<Up>", self.evPreviousCommand)
		self.entryBox.bind("<Down>", self.evNextCommand)
		self.entryBox.bind("<Control-a>", self.evCtrlA)
		self.entryBox.grid(row=0, column=0, sticky=N+S+W+E)
		self.sendButton = Button(self.entryFrame, text='Send', command=self.evSubmitMessage)
		self.sendButton.grid(row=0, column=1)
		self.entryFrame.columnconfigure(0, weight=1)
		self.entryHistory = [""]
		self.entryHistoryIndex = 0
		self.entryBox.focus_set()
		
		# Fix window resizing
		top=self.winfo_toplevel()
		top.rowconfigure(0, weight=1)
		top.columnconfigure(0, weight=1)
		self.rowconfigure(0, weight=1)
		self.columnconfigure(0, weight=1)
		
		# Other stuff
		self.colourRegex = re.compile(r'#[0-9][0-9][0-9][0-9][0-9][0-9]')

	def setStatus(self, msg):
		self.master.title("Rigs of Rods Multiplayer Chat Bot - %s" % (msg))
		
	def evSubmitMessage(self, *args):
		msg = self.entryText.get()
		self.entryText.set("")
		
		self.processMessage(msg)
		
		# Add the message to the history
		self.entryHistory[len(self.entryHistory)-1] = msg
		self.entryHistoryIndex = len(self.entryHistory)
		self.entryHistory.append("")
	
	def evPreviousCommand(self, *args):
		if(self.entryHistoryIndex==len(self.entryHistory)-1):
			self.entryHistory[self.entryHistoryIndex] = self.entryText.get()
			
		self.entryHistoryIndex -= 1
		if(self.entryHistoryIndex<0):
			self.entryHistoryIndex = 0
			return

		self.entryText.set(self.entryHistory[self.entryHistoryIndex])
		
	
	def evNextCommand(self, *args):
		self.entryHistoryIndex += 1
		if(self.entryHistoryIndex==len(self.entryHistory)):
			self.entryHistoryIndex = len(self.entryHistory)-1
			return

		self.entryText.set(self.entryHistory[self.entryHistoryIndex])
	
	def evCtrlA(self, *args):
		self.entryBox.select_range(0, END)
		return "break"
	
	def processMessage(self, msg):
		
		# Trim the message
		msg = msg.strip()
		if(len(msg)==0):
			return
		
		# Split the message into command and arguments
		a = msg.split(' ', 1)
		if len(a)>1:
			args = a[1].split(' ', 5)
		else:
			args = []
		cmd = a[0]

		# Commands start with a '/' character
		if cmd[0] == '/':
			if cmd=="/disconnect":
				self.sendCmd("disconnect", [])
			elif cmd=="/list" or cmd=="/kick" or cmd=="/ban" or cmd=="/unban" or cmd=="/bans" or cmd=="/say":
				self.sendCmd("chat", ['!'+msg[1:]])
			elif cmd=="/whisper":
				if len(args) < 2:
					self.printSystemMessage("Syntax: /whisper <uid> <message>")
				else:
					try:
						args[0] = int(args[0])
					except ValueError:
						self.printSystemMessage("Syntax error, first parameter should be numeric (unique identifier (uid) of user).")
					else:
						self.sendCmd("privchat", [args[0], ''.join(args[1:])])
			elif cmd=="/exit" or cmd=="/quit" or cmd=="/leave":
				self.sendCmd("quit", [])
			else:
				self.printSystemMessage("Unknown command: %s" % (msg))
		else:
			self.sendCmd("chat", [msg])

	def printPublicChatMessage(self, username, message):
		self.addMessage("%s: %s" % (username, message), background="#CCCCFF", selectbackground="#CCCCFF")

	def printPrivateChatMessage(self, username, message):
		self.addMessage("%s(priv): %s" % (username, message), background="#CCCCFF", selectbackground="#CCCCFF")
	
	def printSystemMessage(self, msg):
		self.addMessage(msg, background="#DDEEFF", selectbackground="#DDEEFF")
	
	def printJoinMessage(self, msg):
		self.addMessage(msg, background="#CCFFCC", selectbackground="#CCFFCC")
		
	def printLeaveMessage(self, msg):
		self.addMessage(msg, background="#FFCCCC", selectbackground="#FFCCCC")
	
	def printStreamRegisterMessage(self, msg):
		self.addMessage(msg, background="#DDEEFF", selectbackground="#DDEEFF")
	
	def addMessage(self, message, background="#FFFFFF", foreground="#000000", selectbackground="#FFFFFF", selectforeground="#000000"):
		message = self.colourRegex.sub('', message)
		self.messageBox.insert(END, message)
		self.messageBox.itemconfig(END, background=background, foreground=foreground, selectbackground=selectbackground, selectforeground=selectforeground)
		self.messageBox.yview_moveto(1.0)
	
if __name__ == "__main__":	
	# Create the client
	client = chatbot()




	
