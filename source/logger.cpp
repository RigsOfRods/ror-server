/**
 * Logger Class imlpementation
 * @file logger.cpp
 * @author Christopher Ritchey (aka Aperion) 
 * A simple Logger with different logging levels. As well as an easy to use
 * stack log
 */
#include "logger.h"
#include "pthread.h"
#include "mutexutils.h"

#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdarg>

std::deque <log_save_t> Logger::loghistory;

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

	ctime_r(&lotime, timestr);
	
	// remove trailing new line
	timestr[strlen(timestr)-1]=0;
	
	if (level >= log_level[LOGTYPE_DISPLAY])
		printf("%s|t%02d|%5s|%s\n", timestr, ThreadID::getID(), loglevelname[(int)level], msg.c_str());

	if(file && level >= log_level[LOGTYPE_FILE])
	{
		fprintf(file, "%s|t%02d|%5s| %s\n", timestr, ThreadID::getID(), loglevelname[(int)level], msg.c_str());
		// flushing all the time is a serious performance hit, only flush when
		// we get above the threshold, otherwise let the OS handel flushing 
		if( level >= flush_level )
			fflush(file); // important, as if we crash, we want to be sure to 
			//have the last log entries in the log file!
	}

	if(callback)
	{
		char tmp[2048]="";
		sprintf(tmp, "%s|t%02d|%5s|%s\n", timestr, ThreadID::getID(), loglevelname[(int)level], msg.c_str());
		callback(level, msg, std::string(tmp));
	}

	// save history
	if(level > LOG_STACK)
	{
		if(loghistory.size() > 500)
			loghistory.pop_front();
		log_save_t h;
		h.level = level;
		h.threadid = ThreadID::getID();
		h.time = std::string(timestr);
		h.msg = msg;
		loghistory.push_back(h);
	}
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

void Logger::setFlushLevel(const LogLevel level )
{
	flush_level = level;
}

void Logger::setCallback(void (*ptr)(int, std::string msg, std::string msgf))
{
	callback = ptr;
}

// private:
void (*Logger::callback)(int, std::string msg, std::string msgf) = 0;

Logger::Logger()
{
	setOutputFile("server.log");
}

Logger Logger::theLog;
FILE *Logger::file = 0;
LogLevel Logger::log_level[2] = {LOG_VERBOSE, LOG_INFO};
const char *Logger::loglevelname[] = {"STACK", "DEBUG", "VERBO", "INFO", "WARN", "ERROR"};
LogLevel Logger::flush_level = LOG_ERROR;
bool Logger::compress_file = false;


// SCOPELOG

ScopeLog::ScopeLog(const LogLevel& level, const char* format, ...)
: level(level)
{
    va_list args;
    va_start(args, format);
    msg += format_arg_list(format, args);
    va_end(args);
    
    Logger::log(LOG_STACK, "ENTER - %s", msg.c_str());
}
ScopeLog::ScopeLog(const LogLevel& level, const std::string& func)
: msg(func), level(level)
{
    Logger::log(LOG_STACK, "ENTER - %s", msg.c_str());
}

ScopeLog::~ScopeLog()
{
    Logger::log(LOG_STACK, "EXIT - %s", msg.c_str());
}
