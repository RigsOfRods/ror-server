/**
 * Logger Class Header file
 * @file logger.h
 * @author Christopher Ritchey (aka Aperion) 
 * A simple Logger with different logging levels. As well as an easy to use
 * stack log
 */
#ifndef LOGGER_H_
#define LOGGER_H_

#include "mutexutils.h"

#include "UTFString.h"

#include <string>
#include <deque>


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

typedef struct log_save_t
{
	LogLevel level;
	int threadid;
	UTFString time;
	UTFString msg;
} log_save_t;

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

	static std::deque <log_save_t> getLogHistory();
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
	static std::deque <log_save_t> loghistory;
	static Mutex loghistory_mutex;
};

class ScopeLog
{
public:
    ScopeLog(const LogLevel& level, const char* format, ...);
	ScopeLog(const LogLevel& level, const UTFString& func);
	~ScopeLog();
private:
	UTFString msg;
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
