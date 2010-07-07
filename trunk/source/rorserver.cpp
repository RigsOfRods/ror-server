/*
This file is part of "Rigs of Rods Server" (Relay mode)
Copyright 2007 Pierre-Michel Ricordel
Contact: pricorde@rigsofrods.com
"Rigs of Rods Server" is distributed under the terms of the GNU General Public License.

"Rigs of Rods Server" is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.

"Rigs of Rods Server" is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// RoRserver.cpp : Defines the entry point for the console application.
//
#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"
#include "webserver.h"
#include "messaging.h"

#include "sha1_util.h"
#include "sha1.h"

#include <iostream>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

#include <stdio.h>
#include <string.h>

#ifdef WIN32
# include "windows.h"
# include "resource.h"
#else // WIN32
# include <fcntl.h>
# include <signal.h>
# include <unistd.h>
#endif // WIN32

int terminate_triggered = 0;

void handler(int signalnum)
{
	if(terminate_triggered) return;
	terminate_triggered++;
	// reject handler
	signal(signalnum, handler);

	bool terminate = false;

	if (signalnum == SIGINT)
	{
		Logger::log(LOG_ERROR,"got interrupt signal, terminating ...");
		terminate = true;
	}
	else if (signalnum == SIGTERM)
	{
		Logger::log(LOG_ERROR,"got termiante signal, terminating ...");
		terminate = true;
	}
#ifndef WIN32
	else if (signalnum == SIGHUP)
	{
		Logger::log(LOG_ERROR,"got HUP signal, terminating ...");
		terminate = true;
	}
#endif // ! WIN32
	else
	{
		Logger::log(LOG_ERROR,"got unkown signal: %d", signal);
	}

	if(terminate)
	{
		Sequencer::cleanUp();

		if(Config::getServerMode() == SERVER_LAN)
		{
			Logger::log(LOG_ERROR,"closing server ... ");
			Sequencer::cleanUp();
		}
		else
		{
			Logger::log(LOG_ERROR,"closing server ... unregistering ... ");
			Sequencer::unregisterServer();
			Logger::log(LOG_ERROR," unregistered.");
			Sequencer::cleanUp();
		}
		exit(0);
	}
}

#ifndef WITHOUTMAIN

#ifndef WIN32
// from http://www.enderunix.org/docs/eng/daemon.php
// also http://www-theorie.physik.unizh.ch/~dpotter/howto/daemonize

#define LOCK_FILE "rorserver.lock"

void daemonize()
{
	if(getppid() == 1)
		/* already a daemon */
		return; 
	pid_t pid = fork();
	if (pid < 0) 
	{
		perror("error forking into background");
		exit(1); /* fork error */
	}
	if (pid > 0)
	{
		printf("forked into background as pid %d\n", pid);
		exit(0); /* parent exits */
	}

	/* child (daemon) continues */

	/* Change the file mode mask */
	umask(0);

	/* obtain a new process group */
	pid sid = setsid();
	if (sid < 0)
	{
		perror("unable to get a new session")
		exit(1);
	}

	/* close all descriptors */
	for (i=getdtablesize();i>=0;--i)
		close(i); 

	/* handle standart I/O */
	i=open("/dev/null",O_RDWR); dup(i); dup(i);


	/* Change the current working directory.  This prevents the current
	   directory from being locked; hence not being able to remove it. */
	if ((chdir("/tmp")) < 0)
	{
		perror("unable to change working directory to /tmp")
		exit(1);
	}

	FILE *lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
	if (lfp<0)
	{
		/* can not open */
		perror("could not open lock file");
		exit(1);
	}
	if (lockf(lfp,F_TLOCK,0)<0)
	{
		/* can not lock */
		perror("could not lock");
		exit(0); 
	}
	/* first instance continues */

	/* record pid to lockfile */
	fprintf(lfp, "%d\n",getpid());
	fclose(lfp);

	// ignore some signals
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
}
#endif // ! WIN32

int main(int argc, char* argv[])
{
	// set default verbose levels
	Logger::setLogLevel(LOGTYPE_DISPLAY, LOG_INFO);
	Logger::setLogLevel(LOGTYPE_FILE, LOG_VERBOSE);
	Logger::setFlushLevel(LOG_ERROR);
	Logger::setOutputFile("server.log");


	if( !Config::fromArgs( argc, argv ) )
		return 0;
	if( !Config::checkConfig() )
		return 1;

	// no output because of background mode
	Logger::setLogLevel(LOGTYPE_DISPLAY, LOG_NONE);


	if(!sha1check())
	{
		Logger::log(LOG_ERROR,"sha1 malfunction!");
		exit(-123);
	}

#ifndef WIN32
	if(!Config::getForeground())
	{
		daemonize();
	} else
	{
		// add some logging when using foreground mode
		if(Logger::getLogLevel(LOGTYPE_DISPLAY) == LOG_NONE)
			Logger::setLogLevel(LOGTYPE_DISPLAY, LOG_INFO);
	}
#endif // ! WIN32

	// so ready to run, then set up signal handling
#ifndef WIN32
	signal(SIGHUP, handler);
#endif // ! WIN32
	signal(SIGINT, handler);
	signal(SIGTERM, handler);

	Sequencer::initilize();

	// start webserver if used
	if(Config::getWebserverEnabled())
	{
		int port = Config::getWebserverPort();
		Logger::log(LOG_INFO, "starting webserver on port %d ...", port);
		startWebserver(port);
	}

	// start the main program loop
    // if we need to communiate to the master user the notifier routine
	if(Config::getServerMode() != SERVER_LAN )
	{
		//the main thread is used by the notifier
	    //this should not return untill the server shuts down
		Sequencer::notifyRoutine();
	}
	else
	{
		// if not just idle... forever
		//or by some stupid sleep method in LAN mode
		while (true)
		{
			// update some statistics (handy to use in here, as we have a minute-timer basically)
			Messaging::updateMinuteStats();
			Sequencer::updateMinuteStats();
#ifndef WIN32
			sleep(60);
#else
			Sleep(60*1000);
#endif
		}
	}

	// delete all (needed in here, if not shutdown due to signal)
	// stick in destructor perhaps?
	Sequencer::cleanUp();
	return 0;
}

#endif //WITHOUTMAIN

