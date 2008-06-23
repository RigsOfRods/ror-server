from vector3 import *

CONFIG = {
 'forterrain':  ['nhelensbus'],
 'fortruck':    ['agoras.truck', 'agoral.truck'],
 'name':        'bus_route1',
 'description': 'This is the first ever written MP mission for Nhelens',
}

NHELENS_ROUTE1 = {
	'stop1':{
		'position':   Vector3(974.243, 35.0547, 1037.42), 
		'type':       'busstop',
		'next':       'pass1',
		'name':       'North Coldwater',
		'drivetime':  56.2,
	},
	'pass1':{
		'position':   Vector3(774.915, 42.0934, 1092.75), 
		'type':       'busstop',
		'next':       'stop2',
		'name':       'checkpoint',
		'drivetime':  56.2,
	},
	'stop2':{
		'position':   Vector3( 723.045, 53.5722, 861.426), 
		'next':       None,
		'name':       'Coldwater Harbour',
		'drivetime':  0,
	},
}

class BusMissionTest:
	states = []
	def __init__(self, missionManager):
		self.mm = missionManager
		print "busmission initiated"

	def startPlay(self, missionState):
		self.states.append(missionState)
		msg = "started playing for uid %d" % missionState.client.uid
		self.sendChat2(missionState, msg)
		self.setNewDestionation(missionState, 'stop1')
		
	def sendChat2(self, missionState, msg):
		# this sends a message to the asocciated user
		self.mm.parentClient.sendChat(msg, missionState.client.uid)

	def setDestinationArrow(self, missionState, v, text):
		# this sends a message to the asocciated user
		cmd = "newgoal %d, %f, %f, %f, %s" % (missionState.client.uid, v.x, v.y, v.z, text.replace(' ', '_'))
		print "setDestinationArrow", cmd
		self.mm.parentClient.sendRCon(cmd)
		
	def setNewDestionation(self, missionState, stopname):
		print "setNewDestionation", stopname
		if not stopname in NHELENS_ROUTE1.keys():
			return
		missionState.opts['targetStop'] = stopname
		stop = NHELENS_ROUTE1[stopname]
		self.setDestinationArrow(missionState, stop['position'], stop['name'])
	
	def resetDestination(self, missionState):
		self.mm.parentClient.sendRCon("newgoal %d,0,0,0" % (missionState.client.uid))
		
	def update(self):
		for missionState in self.states:
			if missionState.state == 1:
				#check if near destination
				targetStop = NHELENS_ROUTE1[missionState.opts['targetStop']]
				dest = missionState.client.position.distanceTo(targetStop['position'])
				nextStop = targetStop['next']
				#print dest, nextStop
				if dest < 10:
					if nextStop is None:
						#mission done, deactivate the mission
						self.resetDestination(missionState)
						self.sendChat2(missionState, "mission completed")
						missionState.state=0
					else:
						self.setNewDestionation(missionState, nextStop)


MAINCLASS = None
def startup(missionManager):
	global MAINCLASS
	MAINCLASS = BusMissionTest(missionManager)