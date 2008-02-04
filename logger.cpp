//Logger.cpp

#include "logger.h"

#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <iostream>


// shamelessly taken from:
// http://senzee.blogspot.com/2006/05/c-formatting-stdstring.html   
std::string format_arg_list(const char *fmt, va_list args)
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
    std::string s(buffer);
    delete [] buffer;
    return s;
}

void logmsgf(const LogLevel& level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string s = format_arg_list(format, args);
    va_end(args);
    
    Logger::log(level, s);
}


// public:
Logger::~Logger()
{
    file.close();
}

void Logger::log(const LogLevel& level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string s = format_arg_list(format, args);
    va_end(args);
    
    Logger::log(level, s);
}

void Logger::log(const LogLevel& level, const std::string& msg)
{
	time_t lotime = time(NULL);
	char timestr[50];
	
#ifndef __GNUC__ 
	ctime_s(timestr, 50, &lotime);
#else
	ctime_r(&lotime, timestr);
#endif
	
	// redundant call.... strlen determins the length of the string by
	//	finding the null character
	//timestr[strlen(timestr)-1]=0;

	if( usegui )
	{
#ifdef NCURSES
	    // calls to sequencer do not below in the logger
		WINDOW *win_log = SEQUENCER.getLogWindow();
		if (level>=loglevel)
		{
			wprintw(win_log, "%s: ", timestr);
 			va_list args;
 			va_start(args, format);
			vw_printw(win_log, format, args);
 			va_end(args);
			wprintw(win_log, "\n");
			fflush(stdout);
			wrefresh(win_log);
		}
#endif
	}
	else if (level >= log_level)
		std::cout << timestr << ": " << msg << std::endl;

	if(!file.good())
		return;
	
	file << timestr << ": " << msg << std::endl;
}

void Logger::setGUIMode(const bool& gui)
{
	usegui = gui;
}

void Logger::setOutputFile(const std::string& filename)
{
	file.open( filename.c_str(), std::ios::app & std::ios::out );
}

void Logger::setLogLevel(const LogLevel& level)
{
	log_level = level;
}

// private:
Logger::Logger()
{
	setOutputFile( "server.log" );
}

Logger Logger::theLog;
bool Logger::usegui = false;
LogLevel Logger::log_level = LOG_WARN;
std::fstream Logger::file;
