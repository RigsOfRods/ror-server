/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

// RoRserver.cpp : Defines the entry point for the console application.

#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"
#include "webserver.h"
#include "messaging.h"
#include "listener.h"

#include "sha1_util.h"
#include "sha1.h"

#include <iostream>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
# include "windows.h"
# include "resource.h"
#else // _WIN32
# include <fcntl.h>
# include <signal.h>
# include <unistd.h>
# include <sys/types.h>
# include <pwd.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif // _WIN32

int terminate_triggered = 0;

static Sequencer s_sequencer;

void handler(int signalnum)
{
    if(terminate_triggered) return;
    terminate_triggered++;
    // reject handler
    signal(signalnum, handler);

    bool terminate = false;

    if (signalnum == SIGINT)
    {
        Logger::Log(LOG_ERROR,"got interrupt signal, terminating ...");
        terminate = true;
    }
    else if (signalnum == SIGTERM)
    {
        Logger::Log(LOG_ERROR,"got termiante signal, terminating ...");
        terminate = true;
    }
#ifndef _WIN32
    else if (signalnum == SIGHUP)
    {
        Logger::Log(LOG_ERROR,"got HUP signal, terminating ...");
        terminate = true;
    }
#endif // ! _WIN32
    else
    {
        Logger::Log(LOG_ERROR,"got unkown signal: %d", signal);
    }

    if(terminate)
    {
        if(Config::getServerMode() == SERVER_LAN)
        {
            Logger::Log(LOG_ERROR,"closing server ... ");
            s_sequencer.cleanUp();
        }
        else
        {
            Logger::Log(LOG_ERROR,"closing server ... unregistering ... ");
            s_sequencer.getNotifier()->unregisterServer();
            Logger::Log(LOG_ERROR," unregistered.");
            s_sequencer.cleanUp();
        }
        exit(0);
    }
}

#ifndef WITHOUTMAIN

#ifndef _WIN32
// from http://www.enderunix.org/docs/eng/daemon.php
// also http://www-theorie.physik.unizh.ch/~dpotter/howto/daemonize

#define LOCK_FILE "rorserver.lock"

void daemonize()
{
    if(getppid() == 1)
    {
        /* already a daemon */
        return;
    }

    /* Drop user if there is one, and we were run as root */
    const char *username = "rorserver";
    // TODO: add flexibility to change the username via cmdline
    if ( getuid() == 0 || geteuid() == 0 )
    {
        Logger::Log(LOG_VERBOSE,"changing user to %s", username);
            struct passwd *pw = getpwnam(username);
        if (pw)
        {
            int i = setuid( pw->pw_uid );
            if(i)
            {
                perror("unable to change user");
                exit(1);
            }
        } else
        {
            //perror("error getting user");
            Logger::Log(LOG_ERROR,"unable to get user %s, Is it existing?", username);
            printf("unable to get user %s, Is it existing?\n", username);
            exit(1);
        }
    }

    pid_t pid = fork();
    if (pid < 0) 
    {
        Logger::Log(LOG_ERROR, "error forking into background");
        perror("error forking into background");
        exit(1); /* fork error */
    }
    if (pid > 0)
    {
        // need both here
        printf("forked into background as pid %d\n", pid);
        Logger::Log(LOG_INFO,"forked into background as pid %d", pid);
        exit(0); /* parent exits */
    }

    /* child (daemon) continues */

    /* Change the file mode mask */
    umask(0);

    /* obtain a new process group */
    pid_t sid = setsid();
    if (sid < 0)
    {
        perror("unable to get a new session");
        exit(1);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0)
    {
        perror("unable to change working directory to /");
        exit(1);
    }

    /*
    // TODO: add config option for lockfile name
    {
        int lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
        if (lfp<0)
        {
            //cannot open
            perror("could not open lock file");
            exit(1);
        }
        if (lockf(lfp,F_TLOCK,0)<0)
        {
            // cannot lock
            perror("could not lock");
            exit(0); 
        }

        // record pid to lockfile
        char str[10];
        sprintf(str,"%d\n",getpid());
        write(lfp,str,strlen(str));
    }
    */

    // ignore some signals
    signal(SIGCHLD,SIG_IGN); /* ignore child */
    signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
}
#endif // ! _WIN32

int main(int argc, char* argv[])
{
    // set default verbose levels
    Logger::SetLogLevel(LOGTYPE_DISPLAY, LOG_INFO);
    Logger::SetLogLevel(LOGTYPE_FILE, LOG_VERBOSE);
    Logger::SetOutputFile("server.log");

    if (!Config::fromArgs(argc, argv))
    {
        return 0;
    }

    // If mode == INET and IP not set, this also queries the server for public IP.
    if (!Config::checkConfig())
    {
        return 1;
    }

    if(!sha1check())
    {
        Logger::Log(LOG_ERROR,"sha1 malfunction!");
        return -1;
    }

#ifndef _WIN32
    if(!Config::getForeground())
    {
        // no output because of background mode
        Logger::SetLogLevel(LOGTYPE_DISPLAY, LOG_NONE);
        daemonize();
    }
#endif // ! _WIN32

    // so ready to run, then set up signal handling
#ifndef _WIN32
    signal(SIGHUP, handler);
#endif // ! _WIN32
    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    // Wait for Listener thread to start before registering on the serverlist
    //    (which will contact us back for verification).

    // Yay, oldschool pthreads!
    //    Tutorial: https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables

    pthread_mutex_t listener_ready_mtx;
    pthread_cond_t  listener_ready_cond;
    int             listener_ready_value = 0;

    int mtx_result = pthread_mutex_init(&listener_ready_mtx, nullptr);
    if (mtx_result != 0)
    {
        Logger::Log(LOG_ERROR, "Failed to initialize mutex (listener_ready_mtx), error code: %d", mtx_result);
        return -1;
    }

    int cond_result = pthread_cond_init(&listener_ready_cond, nullptr);
    if (cond_result != 0)
    {
        Logger::Log(LOG_ERROR, "Failed to initialize condition-variable (listener_ready_cond), error code: %d", cond_result);
        return -1;
    }

    Listener* listener = new Listener(&s_sequencer, Config::getListenPort(), &listener_ready_mtx, &listener_ready_cond, &listener_ready_value);
    s_sequencer.initialize(listener);

    // Wait for `Listener` to start up.
    int lock_result = pthread_mutex_lock(&listener_ready_mtx);
    if (lock_result != 0)
    {
        Logger::Log(LOG_ERROR, "Failed to acquire lock, error code: %d", lock_result);
        return -1;
    }
    while (listener_ready_value == 0)
    {
        int wait_result = pthread_cond_wait(&listener_ready_cond, &listener_ready_mtx);
        if (wait_result != 0)
        {
            Logger::Log(LOG_ERROR, "Failed to wait on condition variable, error code: %d", wait_result);
            pthread_mutex_unlock(&listener_ready_mtx);
            return -1;
        }
    }
    pthread_mutex_unlock(&listener_ready_mtx);

    if (listener_ready_value < 0)if (listener_ready_value < 0)
    {
        Logger::Log(LOG_ERROR, "Failed to start up listener, error code: %d", listener_ready_value);
        return -1;
    }

    // Listener is ready, let's register ourselves on serverlist (which will contact us back to check).
    if (Config::getServerMode() != SERVER_LAN)
    {
        s_sequencer.registerServer();
    }
    s_sequencer.activateUserAuth();

#ifdef WITH_WEBSERVER
    // start webserver if used
    if(Config::getWebserverEnabled())
    {
        int port = Config::getWebserverPort();
        Logger::Log(LOG_INFO, "starting webserver on port %d ...", port);
        startWebserver(port);
    }
#endif //WITH_WEBSERVER

    // start the main program loop
    // if we need to communiate to the master user the notifier routine
    if(Config::getServerMode() != SERVER_LAN )
    {
        //the main thread is used by the notifier
        //this should not return untill the server shuts down
        s_sequencer.notifyRoutine();
    }
    else
    {
        // if not just idle... forever
        //or by some stupid sleep method in LAN mode
        while (true)
        {
            // update some statistics (handy to use in here, as we have a minute-timer basically)
            Messaging::updateMinuteStats();
            s_sequencer.updateMinuteStats();

            // broadcast our "i'm here" signal
            Messaging::broadcastLAN();

            // sleep a minute
#ifndef _WIN32
            sleep(60);
#else
            Sleep(60*1000);
#endif
        }
    }

    // delete all (needed in here, if not shutdown due to signal)
    // stick in destructor perhaps?
    s_sequencer.cleanUp();
    return 0;
}

#endif //WITHOUTMAIN

