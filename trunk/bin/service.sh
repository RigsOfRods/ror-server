#!/bin/bash
#
# skeleton	example file to build /etc/init.d/ scripts.
#		This file should be used to construct scripts for /etc/init.d.
#
#		Written by Miquel van Smoorenburg <miquels@cistron.nl>.
#		Modified for Debian GNU/Linux
#		by Ian Murdock <imurdock@gnu.ai.mit.edu>.
#
# Version:	@(#)skeleton  1.8  03-Mar-1998  miquels@cistron.nl
#

### BEGIN INIT INFO
# Provides:          rorserver
# Required-Start:    $remote_fs $syslog $network
# Required-Stop:     $remote_fs $syslog $network
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Rigs of Rods multiplayer server
# Description:       rorserver is a server for the Rigs of Rods Game
### END INIT INFO

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
NAME=rorserver
DESC="Rigs of Rods Multiplayer Server"


USERNAME=rorserver
BASEDIR=/home/rorserver/rorserver-trunk
BINDIR=${BASEDIR}/bin
DAEMON=$BINDIR/rorserver
CONFIG=$BINDIR/servers.conf
LOGDIR=${BINDIR}/logs
PIDDIR=${BINDIR}/pids

[ -x "$DAEMON" ] || exit 0
[ -f "$CONFIG" ] || exit 0

. /lib/lsb/init-functions

case "$1" in
  start)
	# master switch
	count=1
	port=12000
	while read ARGS
	do
		start-stop-daemon --start --background --user $USERNAME --name rorserver-${count} -v --exec $DAEMON --make-pidfile --pidfile $PIDDIR/server-${count}.pid  -- -name Official_${count} -port ${port} -logfilename $LOGDIR/server-${count}.log $ARGS 
		ret=$?
		#log_progress_msg "$ARGS"
		log_end_msg $ret

		count=$((count+1))
		port=$((port+1))
	done < $CONFIG
	;;
  stop)
	FILES=$(ls $PIDDIR/*.pid 2>/dev/null)
	if [[ "$FILES" == "" ]]
	then
		log_daemon_msg "no rorservers detected running"
		log_end_msg 1
		exit 0
	fi
	for PID in $FILES
	do
 		log_daemon_msg "Stopping $NAME $PID"
		start-stop-daemon --stop --user $USERNAME --pidfile $PID --retry=TERM/30/KILL/5
		log_end_msg $?
		rm $PID >>/dev/null 2>&1
	done
	;;
  reload|restart|force-reload)
	$0 stop && $0 start
	;;
  *)
	echo "Usage: $0 {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
