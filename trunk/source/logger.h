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
#include <deque>


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

typedef struct log_save_t
{
	LogLevel level;
	int threadid;
	std::string time;
	std::string msg;
} log_save_t;

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
	//! sets the level at which manual file flushing occurs flushing at the 
	//! stack level is a heavy hit on performance with only 2 clients connected
	//! This prevents any messages below the specified level triggering manual
	//! flushing.
	//! if set to LOG_ERROR then flushing occurs only when an error is logged
	static void setFlushLevel(const LogLevel level );

	static void setCallback(void (*ptr)(int, std::string msg, std::string msgf));

	static std::deque <log_save_t> getLogHistory() { return loghistory; };
	static const char *getLoglevelName(int i) { return loglevelname[i]; };
private:
	Logger();
	Logger instance();
	static Logger theLog;
	static FILE *file;
	static LogLevel log_level[2];
	static const char *loglevelname[];
	static LogLevel flush_level;
	static bool compress_file;
	static void (*callback)(int, std::string msg, std::string msgf);
	static std::deque <log_save_t> loghistory;
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

#ifndef NOSTACKLOG
#define STACKLOG ScopeLog stacklog( LOG_STACK, __PRETTY_FUNCTION__ )
#else
#define STACKLOG {}
#endif

#endif // LOGGER_H_
