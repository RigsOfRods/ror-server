//Logger.cpp

#include "logger.h"

#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdarg>


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
    if(file)
		fclose(file);
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
	memset(timestr, 0, 50);
	
#ifndef __GNUC__ 
	ctime_s(timestr, 50, &lotime);
#else
	ctime_r(&lotime, timestr);
#endif
	// this is not redundant as we remove the trailing \n --thomas
	timestr[strlen(timestr)-1]=0;

	if (level >= log_level)
		printf("%s|%5s|%s\n", timestr, loglevelname[(int)level], msg.c_str());

	if(file)
		fprintf(file, "%s|%5s| %s\n", timestr, loglevelname[(int)level], msg.c_str());
}

void Logger::setOutputFile(const std::string& filename)
{
	if(file)
		fclose(file);
	file = fopen(filename.c_str(), "a");
}

void Logger::setLogLevel(const LogLevel& level)
{
	log_level = level;
}

// private:
Logger::Logger()
{
	setOutputFile("server.log");
}

Logger Logger::theLog;
bool Logger::usegui = false;
LogLevel Logger::log_level = LOG_WARN;
FILE *Logger::file = 0;
char *Logger::loglevelname[5]= {"DEBUG", "VERBO", "INFO", "WARN", "ERROR"};