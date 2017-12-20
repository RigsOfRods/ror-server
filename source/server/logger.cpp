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

static FILE *s_file = nullptr;
static LogLevel s_log_level[2] = {LOG_VERBOSE, LOG_INFO};
static const char *s_log_level_names[] = {"STACK", "DEBUG", "VERBO", "INFO", "WARN", "ERROR"};
static std::string s_log_filename = "server.log";
static Mutex s_log_mutex;

// take care about mutexes: only manual lock in the Logger, otherwise you 
// could possibly start a recursion that ends in a deadlock

// shamelessly taken from http://senzee.blogspot.com/2006/05/c-formatting-stdstring.html
std::string format_arg_list(const char *fmt, va_list args) {
    if (!fmt) {
        return "";
    }
    int result = -1, length = 256;
    char *buffer = 0;
    while (result == -1) {
        if (buffer) {
            delete[] buffer;
        }
        buffer = new char[length + 1];
        memset(buffer, 0, length + 1);
        result = vsnprintf(buffer, length, fmt, args);
        length *= 2;
    }
    std::string s = Str::SanitizeUtf8(buffer);
    delete[] buffer;
    return s;
}

namespace Logger {

    void Log(const LogLevel &level, const char *format, ...) {
        va_list args;
        va_start(args, format);
        Logger::Log(level, format_arg_list(format, args));
        va_end(args);
    }

    void Log(const LogLevel &level, const std::string &msg) {
        time_t current_time = time(nullptr);
        char time_str[] = "DD-MM-YYYY hh:mm:ss"; // Placeholder
        strftime(time_str, 20, "%d-%m-%Y %H:%M:%S", localtime(&current_time));
        const char *level_str = s_log_level_names[(int) level];

        if (level >= s_log_level[LOGTYPE_DISPLAY]) {
            printf("%s|t%02d|%5s|%s\n", time_str, ThreadID::getID(), level_str, msg.c_str());
        }

        // do not use the class for locking, otherwise you get recursion because of STACKLOG
        // UPDATE: 2016/05 only_a_ptr: STACKLOG was removed, TODO verify this
        pthread_mutex_lock(s_log_mutex.getRaw());

        if (s_file && level >= s_log_level[LOGTYPE_FILE]) {
/* FIXME If you need this feature, use copytruncate option for logrotate for now
#ifndef _WIN32
        
        // check if we need to reopen the s_file (i.e. moved by logrotate)
        struct stat mystat;
        if (stat(s_log_filename.asUTF8_c_str(), &mystat))
        {		
            freopen(s_log_filename.asUTF8_c_str(), "a+", s_file);
        }
#endif // _WIN32
*/
            fprintf(s_file, "%s|t%02d|%5s| %s\n", time_str, ThreadID::getID(), level_str, msg.c_str());
            fflush(s_file);
        }

        pthread_mutex_unlock(s_log_mutex.getRaw());
    }

    void SetOutputFile(const std::string &filename) {
        s_log_filename = filename;
        if (s_file) {
            fclose(s_file);
        }
        s_file = fopen(s_log_filename.c_str(),
                       "a+"); // FIXME: This will fail on Windows, UTF-8 paths are not supported.
        // TODO Windows: research and convert the path to UTF-16

        fprintf(s_file, "%s\n", "============================== RoR-Server started ==============================");
    }

    void SetLogLevel(const LogType type, const LogLevel level) {
        s_log_level[(int) type] = level;
    }

} // namespace Logger
