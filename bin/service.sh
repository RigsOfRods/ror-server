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
  start|restartbroken)
	count=0
	port=12000
	while read ARGS
	do
		count=$((count+1))
		port=$((port+1))
		NUMFOR=$(printf "%02d" ${count})
		PIDFILE=$PIDDIR/server-${NUMFOR}.pid
		PIDNUM=$(cat $PIDFILE)
		# check if we only start a certain server
		if [ -n "$2" ]
		then
			if [[ "$NUMFOR" != "$2" ]] ; then
				continue
			fi
		fi
		if [[ -f $PIDFILE ]]
		then
	                #start-stop-daemon --signal 0 --stop --user $USERNAME --pidfile $PIDFILE  >>/dev/null 2>&1
	                #if [[ "${?}" != "0" ]]
			RUNNING=$(ps -p ${PIDNUM} | wc -l)
			if [ "$RUNNING" != "2" ]
			then
				rm -f $PIDFILE >>/dev/null 2>&1
				log_daemon_msg "broken, restarting"
			else
				if [[ "$1" != "restartbroken" ]]
				then 
					log_daemon_msg " already running"
					log_end_msg 1
				fi
				continue
			fi
		fi
		log_daemon_msg "Starting $NAME Server ${count}"
		start-stop-daemon --start --background --user $USERNAME --name rorserver-${NUMFOR} --exec $DAEMON --make-pidfile --pidfile $PIDFILE  -- -name Official_${NUMFOR}  -port ${port} -logfilename $LOGDIR/server-${NUMFOR}.log -logverbosity 1 $ARGS 
		log_end_msg 0 

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
  checkrestart)
        FILES=$(ls $PIDDIR/*.pid 2>/dev/null)
        if [[ "$FILES" == "" ]]
        then
                log_daemon_msg "no rorservers detected running"
                log_end_msg 1
                exit 0
        fi
	mem_total=0
        for PIDFILE in $FILES
        do
		PIDNUM=$(cat $PIDFILE)
		RUNNING=$(ps -p ${PIDNUM} | wc -l)
		#start-stop-daemon --signal 0 --stop --user $USERNAME --pidfile $PIDFILE >>/dev/null 2>&1
                #if [ "${?}" -ne "0" ]
                if [ "$RUNNING" != "2" ]
		then
			SERVERNUM=$(basename ${PIDFILE})
			SERVERNUM=${SERVERNUM:7:2}
			echo "server $SERVERNUM not running ..."
		fi
        done
        ;;	

  check)
        FILES=$(ls $PIDDIR/*.pid 2>/dev/null)
        if [[ "$FILES" == "" ]]
        then
                log_daemon_msg "no rorservers detected running"
                log_end_msg 1
                exit 0
        fi
	mem_total=0
        for PIDFILE in $FILES
        do
		PIDNUM=$(cat $PIDFILE)
                log_daemon_msg "Checking ${NAME} ($(basename ${PIDFILE}))[${PIDNUM}]"
		RUNNING=$(ps -p ${PIDNUM} | wc -l)
                if [ "$RUNNING" != "2" ]
		then
			echo "server $SERVERNUM not running ..."
			continue
		fi
		
                #start-stop-daemon --signal 0 --stop --user $USERNAME --pidfile $PIDFILE
		MEM=$(pmap -x $PIDNUM | grep total | awk '{print $3}')
		((MEMMB=MEM/1024))
		log_daemon_msg "memory: $MEM KB / $MEMMB MB"
                log_end_msg $?
		((mem_total=mem_total+MEM))
        done
	((mem_total=mem_total/1024))
	echo "total used memory: $mem_total MB"
        ;;	
  reload|restart|force-reload)
	$0 stop && $0 start
	;;
  *)
	echo "Usage: $0 {start|stop|restart|force-reload|check|checkrestart|restartbroken}" >&2
	exit 1
	;;
esac

exit 0
