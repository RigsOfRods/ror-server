import logging

class DataPacket:
	source=0
	command=0
	streamid=0
	size=0
	data=0
	time=0
	def __init__(self, command, source, streamid, size, data):
		self.source = source
		self.command = command
		self.streamid = streamid
		self.size = size
		self.data = data
		self.time = 0

		
class Logger(object):
	def __init__(self, thread_id):
		self.thread_id = thread_id
		logging.basicConfig(level=logging.DEBUG, format='%(name)s: %(message)s')
		
		logging.basicConfig(level=logging.DEBUG,
						format='%(asctime)s %(name)-12s %(levelname)-8s %(message)s',
						datefmt='%m.%d %H:%M',
						filename='log-%d.txt'%thread_id,
						filemode='w')
		self.log = logging.getLogger('client-%02d' % thread_id)

	def write(self, message):
		msg = message.strip()
		if msg == '':
			return
		self.log.debug(msg)  

