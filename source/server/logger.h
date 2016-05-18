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

#pragma once

#include "mutexutils.h"

#include "UTFString.h"

#include <string>

enum LogLevel
{
    LOG_STACK=0,
    LOG_DEBUG,
    LOG_VERBOSE,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_NONE
};

enum LogType
{
    LOGTYPE_FILE=0,
    LOGTYPE_DISPLAY
};

//extern int loglevel;
void logmsgf(const LogLevel& level, const char* format, ...);

class Logger
{
public:
    virtual ~Logger();

    static void log(const LogLevel& level, const char* format, ...);
    static void log(const LogLevel& level, const UTFString& msg);
    
    static void setOutputFile(const UTFString& filename);
    static void setLogLevel(const LogType type, const LogLevel level);
    static const LogLevel getLogLevel(const LogType type);

    static void setCallback(void (*ptr)(int, UTFString msg, UTFString msgf));

    static const char *getLoglevelName(int i) { return loglevelname[i]; };
private:
    Logger();
    Logger instance();
    static Logger theLog;
    static FILE *file;
    static LogLevel log_level[2];
    static const char *loglevelname[];
    static UTFString logfilename;
    static bool compress_file;
    static void (*callback)(int, UTFString msg, UTFString msgf);
    static Mutex log_mutex;
};
