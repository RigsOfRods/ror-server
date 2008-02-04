#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>
#include <fstream>


enum LogLevel {
	LOG_VVERBOSE,
	LOG_VERBOSE,
	LOG_DEBUG,
	LOG_WARN,
	LOG_ERROR
};

//extern int loglevel;
void logmsgf(const LogLevel& level, const char* format, ...);

class Logger
{
public:
	virtual ~Logger();
	
	static void log(const LogLevel& level, const char* format, ...);
	static void log(const LogLevel& level, const std::string& msg);
	
	static void setGUIMode(const bool& gui);
	static void setOutputFile(const std::string& filename);
	static void setLogLevel(const LogLevel& level);
	
private:
	Logger();
	Logger instance();
	static Logger theLog;
	static bool usegui;
	static LogLevel log_level;
	static std::fstream file;
};

class ScopeLog
{
public:
	ScopeLog(std::string func)
	{
		msg = func;
		logmsgf(LOG_VVERBOSE, "%s - %s", "ENTER", msg.c_str());
	}
	
	~ScopeLog()
	{
		logmsgf(LOG_VVERBOSE, "%s - %s", "EXIT", msg.c_str());
	}
private:
	std::string msg;
};

#define STACKLOG Logger::log( LOG_VVERBOSE, __PRETTY_FUNCTION__ )

#endif // LOGGER_H_
