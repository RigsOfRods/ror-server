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
    Logger::log(level, format_arg_list(format, args));
    va_end(args);
}


// public:
Logger::~Logger()
{
	if(file)
		fclose(file);
}

#if 0
void Logger::log(const LogLevel& level, const char* format, va_list args)
{
    std::string s = format_arg_list(format, args);
    Logger::log(level, s);   
}
#endif

void Logger::log(const LogLevel& level, const char* format, ...)
{
	va_list args;
	va_start(args, format);
    Logger::log(level, format_arg_list(format, args));
	va_end(args);
}

void Logger::log(const LogLevel& level, const std::string& msg)
{
	time_t lotime = time(NULL);
	char timestr[50];
	memset(timestr, 0, 50);

	// if compiling without gcc this is a macro
	ctime_r(&lotime, timestr);
	
	// remove trailing new line
	timestr[strlen(timestr)-1]=0;

	if (level >= log_level[LOGTYPE_DISPLAY])
		printf("%s|%5s|%s\n", timestr, loglevelname[(int)level], msg.c_str());

	if(file && level >= log_level[LOGTYPE_FILE])
		fprintf(file, "%s|%5s| %s\n", timestr, loglevelname[(int)level], msg.c_str());
}

void Logger::setOutputFile(const std::string& filename)
{
	if(file)
		fclose(file);
	file = fopen(filename.c_str(), "a");
}

void Logger::setLogLevel(const LogType type, const LogLevel level)
{
	log_level[(int)type] = level;
}

// private:
Logger::Logger()
{
	setOutputFile("server.log");
}

Logger Logger::theLog;
FILE *Logger::file = 0;
LogLevel Logger::log_level[2] = {LOG_VERBOSE, LOG_INFO};
char *Logger::loglevelname[] = {"STACK", "DEBUG", "VERBO", "INFO", "WARN", "ERROR"};


// SCOPELOG

ScopeLog::ScopeLog(const LogLevel& level, const char* format, ...)
: level(level)
{
    va_list args;
    va_start(args, format);
    msg += format_arg_list(format, args);
    va_end(args);
    
    Logger::log(LOG_DEBUG, "STACK|ENTER - %s", msg.c_str());
}
ScopeLog::ScopeLog(const LogLevel& level, const std::string& func)
: msg(func), level(level)
{
    Logger::log(LOG_DEBUG, "STACK|ENTER - %s", msg.c_str());
}

ScopeLog::~ScopeLog()
{
    Logger::log(LOG_DEBUG, "STACK|EXIT - %s", msg.c_str());
}
