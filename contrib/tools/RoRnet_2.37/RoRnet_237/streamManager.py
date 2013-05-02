from utils import *
from protocol import *
import time

class user_t:
	def __init__(self, user_info):
		self.user = user_info
		self.stream = {}
		self.stats = user_stats_t()
		
class user_stats_t:
	def __init__(self):
		self.onlineSince       = time.time()
		self.currentStream   = {'uniqueID': -1, 'streamID': -1}
		self.characterStreamID = -1
		self.chatStreamID      = -1
		self.distanceDriven    = 0.0
		self.distanceSailed    = 0.0
		self.distanceWalked    = 0.0
		self.distanceFlown     = 0.0
	
def getTruckType(filename):
	return filename.split('.').pop().lower()

class streamManager:
	
	def __init__(self):		
		self.D = {}
		self.globalStats = {
			'connectTime': time.time(),
			'distanceDriven': long(0),
			'distanceSailed': long(0),
			'distanceWalked': long(0),
			'distanceFlown': long(0),
			'usernames': set(),
			'userCount': 0,
			'connectTimes': list()
		}
		
		# Add the server itself as a client (hacky hack)
		u = user_info_t()
		u.username = "server"
		u.uniqueID = -1
		u.authstatus = AUTH_BOT
		self.updateClient(u)

	# Updates or adds a client
	def updateClient(self, user_info):
		if not user_info.uniqueID in self.D:
			self.D[user_info.uniqueID] = user_t(user_info)
			self.globalStats['usernames'].add(user_info.username)
			self.globalStats['userCount'] += 1
			return "added"
		else:
			self.D[user_info.uniqueID].user.update(user_info)
			self.globalStats['usernames'].add(user_info.username)
			return "updated"
	
	def delClient(self, uid):
		if uid in self.D:
			self.globalStats['distanceDriven'] += self.D[uid].stats.distanceDriven
			self.globalStats['distanceSailed'] += self.D[uid].stats.distanceSailed
			self.globalStats['distanceWalked'] += self.D[uid].stats.distanceWalked
			self.globalStats['distanceFlown']  += self.D[uid].stats.distanceFlown
			self.globalStats['connectTimes'].append(time.time()-self.D[uid].stats.onlineSince)
			del self.D[uid]
	
	# s: stream_info_t
	def addStream(self, s):
		if s.origin_sourceid in self.D:
			s.fileExt = getTruckType(s.name)
			self.D[s.origin_sourceid].stream[s.origin_streamid] = s

			if s.type == TYPE_CHARACTER:
				self.setCharSID(s.origin_sourceid, s.origin_streamid)
			elif s.type == TYPE_CHAT:
				self.setChatSID(s.origin_sourceid, s.origin_streamid)
	
	def delStream(self, uid, sid):
		if uid in self.D and sid in self.D[uid].stream:
			if self.D[uid].stream[sid].origin_streamid == self.D[uid].stats.characterStreamID:
				self.D[uid].stats.characterStreamID = -1
			elif self.D[uid].stream[sid].origin_streamid == self.D[uid].stats.chatStreamID:
				self.D[uid].stats.chatStreamID = -1
			del self.D[uid].stream[sid]

	def setPosition(self, uid, sid, pos):
		if uid in self.D and sid in self.D[uid].stream:
			dist = squaredLengthBetween2Points(pos, self.D[uid].stream[sid].refpos)
			if dist<10.0:
				if self.D[uid].stream[sid].type == TYPE_CHARACTER:
					self.D[uid].stats.distanceWalked += dist
				elif self.D[uid].stream[sid].fileExt == "truck":
					self.D[uid].stats.distanceDriven += dist
				elif self.D[uid].stream[sid].fileExt == "airplane":
					self.D[uid].stats.distanceFlown += dist
				elif self.D[uid].stream[sid].fileExt == "boat":
					self.D[uid].stats.distanceSailed += dist
			self.D[uid].stream[sid].refpos = pos

	def getPosition(self, uid, sid = -1):
		if sid == -1:
			return self.getCurrentStream(uid).refpos
		elif uid in self.D and sid in self.D[uid].stream:
			return self.D[uid].stream[sid].refpos
		else:
			return vector3()

	def setRotation(self, uid, sid, rot):
		if uid in self.D and sid in self.D[uid].stream:
			self.D[uid].stream[sid].rot = rot		

	def getRotation(self, uid, sid):
		if uid in self.D and sid in self.D[uid].stream:
			return self.D[uid].stream[sid].rot
		else:
			return vector4()
	
	def setCurrentStream(self, uid_person, uid_truck, sid):
		if uid_person in self.D and uid_truck in self.D and sid in self.D[uid_truck].stream:
			self.D[uid_person].stats.currentStream = {'uniqueID': uid_truck, 'streamID': sid}
			if sid != self.D[uid_person].stats.characterStreamID or uid_person != uid_truck:
				self.setPosition(uid_person, self.D[uid_person].stats.characterStreamID, vector3())

	def getCurrentStream(self, uid):
		if uid in self.D:
			if self.D[uid].stats.currentStream['uniqueID'] in self.D and self.D[uid].stats.currentStream['streamID'] in self.D[self.D[uid].stats.currentStream['uniqueID']].stream:
				return self.D[self.D[uid].stats.currentStream['uniqueID']].stream[self.D[uid].stats.currentStream['streamID']]
		return stream_info_t()
	
	def setCharSID(self, uid, sid):
		if uid in self.D and sid in self.D[uid].stream:
			self.D[uid].stats.characterStreamID = sid

	def getCharSID(self, uid):
		if uid in self.D:
			print "getting charSID %d" % self.D[uid].stats.characterStreamID
			return self.D[uid].stats.characterStreamID	
		else:
			return -1

	def setChatSID(self, uid, sid):
		if uid in self.D and sid in self.D[uid].stream:
			self.D[uid].stats.chatStreamID = sid

	def getChatSID(self, uid):
		if uid in self.D:
			return self.D[uid].stats.chatStreamID
		else:
			return -1

	def getOnlineSince(self, uid):
		if uid in self.D:
			return self.D[uid].stats.onlineSince
		else:
			return 0.0
	
	def countClients(self):
		return len(self.D)-1 # minus one because, internally, we consider the server to be a user
	
	def countStreams(self, uid):
		if uid in self.D:
			return len(self.D[uid].stream)
		else:
			return 999999
			
	def getUsernameColoured(self, uid):
		try:
			return "%s%s%s" % (playerColours[self.D[uid].user.colournum], self.D[uid].user.username, COLOUR_BLACK)
		except:
			return "%s%s" % (self.D[uid].user.username, COLOUR_BLACK);
	
	def getUsername(self, uid):
		if uid in self.D:
			return self.D[uid].user.username
		else:
			return "unknown(%d)" % (uid)

	def getAuth(self, uid):
		if uid in self.D:
			return self.D[uid].user.authstatus
		else:
			return AUTH_NONE

	def getClientName(self, uid):
		if uid in self.D:
			return self.D[uid].user.clientname
		else:
			return "unknown(%d)" % (uid)

	def getClientVersion(self, uid):
		if uid in self.D:
			return self.D[uid].user.clientversion
		else:
			return "0.00"

	def getLanguage(self, uid):
		if uid in self.D:
			return self.D[uid].user.language
		else:
			return "xx_XX"
	
	def userExists(self, uid):
		return uid in self.D
	
	def getSessionType(self, uid):
		if uid in self.D:
			return self.D[uid].user.sessiontype
		else:
			return ""
	
	def getStreamData(self, uid, sid):
		if uid in self.D and sid in self.D[uid].stream:
			return self.D[uid].stream[sid]
		else:
			return stream_info_t()
	
	def getUserData(self, uid):
		if uid in self.D:
			return self.D[uid].user
		else:
			return user_info_t()
	
	def getStats(self, uid = None):
		if uid is None:
			return self.globalStats
		elif uid in self.D:
			return self.D[uid].stats
		else:
			return user_stats_t()
