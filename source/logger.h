/**
 * Logger Class Header file
 * @file logger.h
 * @author Christopher Ritchey (aka Aperion) 
 * A simple Logger with different logging levels. As well as an easy to use
 * stack log
 */
#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>


enum LogLevel
{
	LOG_STACK=0,
	LOG_DEBUG,
	LOG_VERBOSE,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR
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
	static void log(const LogLevel& level, const std::string& msg);
	
	static void setOutputFile(const std::string& filename);
	static void setLogLevel(const LogType type, const LogLevel level);
	
private:
	Logger();
	Logger instance();
	static Logger theLog;
	static FILE *file;
	static LogLevel log_level[2];
	static const char *loglevelname[];
};

class ScopeLog
{
public:
    ScopeLog(const LogLevel& level, const char* format, ...);
	ScopeLog(const LogLevel& level, const std::string& func);
	~ScopeLog();
private:
	std::string msg;
	const LogLevel level;
};


// macros for crossplatform compiling
#ifndef __GNUC__
	#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#define STACKLOG ScopeLog stacklog( LOG_STACK, __PRETTY_FUNCTION__ )
#endif // LOGGER_H_
