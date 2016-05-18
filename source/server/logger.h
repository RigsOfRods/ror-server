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

#include "UTFString.h"

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

namespace Logger {

void Log(const LogLevel& level, const char* format, ...);
void Log(const LogLevel& level, const UTFString& msg);

void SetOutputFile(const UTFString& filename);
void SetLogLevel(const LogType type, const LogLevel level);

} // namespace Logger
