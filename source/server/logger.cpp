/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2008   Christopher Ritchey (aka Aperion)
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

#include "logger.h"

#include "pthread.h"
#include "utils.h"
#include "mutexutils.h"

#include <stdio.h>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdarg>

#ifndef _WIN32
#include <sys/stat.h>
#endif //_WIN32

static FILE *file = nullptr;
static LogLevel log_level[2] = { LOG_VERBOSE, LOG_INFO };
static const char *loglevelname[] = { "STACK", "DEBUG", "VERBO", "INFO", "WARN", "ERROR" };
static UTFString logfilename = "server.log";
static bool compress_file = false;
static Mutex log_mutex;

// take care about mutexes: only manual lock in the Logger, otherwise you 
// could possibly start a recursion that ends in a deadlock

// shamelessly taken from:
// http://senzee.blogspot.com/2006/05/c-formatting-stdstring.html
UTFString format_arg_list(const char *fmt, va_list args)
{
    if (!fmt) return "";
    int   result = -1, length = 256;
    char *buffer = 0;
    while (result == -1)
    {
        if (buffer) delete [] buffer;
        buffer = new char [length + 1];
        memset(buffer, 0, length + 1);
        result = vsnprintf(buffer, length, fmt, args);
        length *= 2;
    }
    UTFString s = tryConvertUTF(buffer);
    delete [] buffer;
    return s;
}

namespace Logger {

void Log(const LogLevel& level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger::Log(level, format_arg_list(format, args));
    va_end(args);
}

void Log(const LogLevel& level, const UTFString& msg)
{
    time_t current_time = time(nullptr);
    char time_str[] = "DD-MM-YYYY hh:mm:ss"; // Placeholder
    strftime(time_str, 20, "%d-%m-%Y %H:%M:%S", localtime(&current_time));

    if (level >= log_level[LOGTYPE_DISPLAY])
    {
        printf("%s|t%02d|%5s|%s\n", time_str, ThreadID::getID(), loglevelname[(int)level], msg.asUTF8_c_str());
    }

    // do not use the class for locking, otherwise you get recursion because of STACKLOG
    // UPDATE: 2016/05 only_a_ptr: STACKLOG was removed, TODO verify this
    pthread_mutex_lock(log_mutex.getRaw());

    if(file && level >= log_level[LOGTYPE_FILE])
    {
/* FIXME If you need this feature, use copytruncate option for logrotate for now
#ifndef _WIN32
        
        // check if we need to reopen the file (i.e. moved by logrotate)
        struct stat mystat;
        if (stat(logfilename.asUTF8_c_str(), &mystat))
        {		
            freopen(logfilename.asUTF8_c_str(), "a+", file);
        }
#endif // _WIN32
*/
        fprintf(file, "%s|t%02d|%5s| %s\n", time_str, ThreadID::getID(), loglevelname[(int)level], msg.asUTF8_c_str());
        fflush(file);
    }

    pthread_mutex_unlock(log_mutex.getRaw());
}

void SetOutputFile(const UTFString& filename)
{
    logfilename = filename;
    if(file)
        fclose(file);
    file = fopen(logfilename.asUTF8_c_str(), "a+");
}

void SetLogLevel(const LogType type, const LogLevel level)
{
    log_level[(int)type] = level;
}

} // namespace Logger
