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

#include "utils.h"

#include <stdio.h>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <sstream>
#include <thread>

#ifndef _WIN32

#include <sys/stat.h>

#endif //_WIN32

static FILE *s_file = nullptr;
static LogLevel s_log_level[2] = {LOG_VERBOSE, LOG_INFO};
static const char *s_log_level_names[] = {"STACK", "DEBUG", "VERBO", "INFO", "WARN", "ERROR"};
static std::string s_log_filename = "server.log";
static std::mutex s_log_mutex;

namespace Logger {

    void Log(LogLevel level, const char *format, ...) {
        // Format the message
        const int BUF_LEN = 4000; // hard limit
        char buffer[BUF_LEN] = {}; // zeroed memory
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, BUF_LEN, format, args);
        va_end(args);
        // Log the message
        LogWrite(level, buffer);
    }

    void Log(LogLevel level, std::string const& msg) {
        LogWrite(level, msg.c_str());
    }

    void LogWrite(LogLevel level, const char* msg) {
        time_t current_time = time(nullptr);
        char time_str[] = "DD-MM-YYYY hh:mm:ss"; // Placeholder
        strftime(time_str, 20, "%d-%m-%Y %H:%M:%S", localtime(&current_time));
        const char *level_str = s_log_level_names[(int) level];

        std::stringstream tid_ss;
        tid_ss << std::this_thread::get_id();
        std::string tid_str = tid_ss.str();

        if (level >= s_log_level[LOGTYPE_DISPLAY]) {
            printf("%s|t%s|%5s|%s\n", time_str, tid_str.c_str(), level_str, msg);
        }

        std::lock_guard<std::mutex> scoped_lock(s_log_mutex);

        if (s_file && level >= s_log_level[LOGTYPE_FILE]) {
#ifndef _WIN32
        
        // check if we need to reopen the s_file (i.e. moved by logrotate)
        struct stat mystat;
        if (stat(s_log_filename.c_str(), &mystat))
        {
            freopen(s_log_filename.c_str(), "a+", s_file);
        }
#endif // _WIN32

            fprintf(s_file, "%s|t%s|%5s| %s\n", time_str, tid_str.c_str(), level_str, msg);
            fflush(s_file);
        }
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
