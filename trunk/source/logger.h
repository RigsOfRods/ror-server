#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>


enum LogLevel
{
	LOG_DEBUG=0,
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

#if 0
	static void log(const LogLevel& level, const char* format, va_list args);
#endif
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
	static char *loglevelname[5];
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
	#define ctime_r(lotime, timestr) ctime_s(timestr, 50, &lotime);
#endif

#define STACKLOG ScopeLog stacklog( Logger::LOG_STACK, __PRETTY_FUNCTION__ )
#endif // LOGGER_H_
