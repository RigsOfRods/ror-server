#include "config.h"

// simpleopt by http://code.jellycan.com/simpleopt/
// license: MIT
#include "SimpleOpt.h"
#include "SocketW.h"
#include "rornet.h"
#include "HttpMsg.h"
#include "logger.h"
#include "sequencer.h"
#include "sha1_util.h"

#include <cmath>

#ifdef WIN32
#include <windows.h>
#include <time.h>
#endif

#ifdef __GNUC__
#include <unistd.h>
#include <stdlib.h>
#endif

#include <cstdio>

#ifndef NOCMDLINE

// option identifiers
enum
{
	OPT_HELP,
	OPT_IP,
	OPT_PORT,
	OPT_NAME,
	OPT_TERRAIN,
	OPT_MAXCLIENTS,
	OPT_LAN,
	OPT_VERBOSITY,
	OPT_LOGVERBOSITY,
	OPT_PASS,
	OPT_INET,
	OPT_LOGFILENAME,
	OPT_SCRIPTNAME,
	OPT_WEBSERVER,
	OPT_WEBSERVER_PORT,
	OPT_VERSION,
	OPT_FOREGROUND,
	OPT_CONFIGFILE
};

// option array
static CSimpleOpt::SOption cmdline_options[] = {
	{ OPT_IP,             ((char *)"-ip"),   SO_REQ_SEP },
	{ OPT_PORT,           ((char *)"-port"), SO_REQ_SEP },
	{ OPT_NAME,           ((char *)"-name"), SO_REQ_SEP },
	{ OPT_PASS,           ((char *)"-password"), SO_REQ_SEP },
	{ OPT_TERRAIN,        ((char *)"-terrain"), SO_REQ_SEP },
	{ OPT_MAXCLIENTS,     ((char *)"-maxclients"), SO_REQ_SEP },
	{ OPT_LAN,            ((char *)"-lan"), SO_NONE },
	{ OPT_INET,           ((char *)"-inet"), SO_NONE },
	{ OPT_VERBOSITY,      ((char *)"-verbosity"), SO_REQ_SEP },
	{ OPT_LOGVERBOSITY,   ((char *)"-logverbosity"), SO_REQ_SEP },
	{ OPT_LOGFILENAME,    ((char *)"-logfilename"), SO_REQ_SEP },
	{ OPT_SCRIPTNAME,     ((char *)"-script"), SO_REQ_SEP },
	{ OPT_WEBSERVER,      ((char *)"-webserver"), SO_NONE },
	{ OPT_WEBSERVER_PORT, ((char *)"-webserverport"), SO_REQ_SEP },
	{ OPT_VERSION,        ((char *)"-version"), SO_NONE },
	{ OPT_HELP,           ((char *)"-help"), SO_NONE },
	{ OPT_HELP,           ((char *)"--help"), SO_NONE },
	{ OPT_FOREGROUND,     ((char *)"-fg"), SO_NONE },
	{ OPT_CONFIGFILE,     ((char *)"-c"), SO_REQ_SEP },
	{ OPT_CONFIGFILE,     ((char *)"-config"), SO_REQ_SEP },
	{ OPT_HELP,           ((char *)"/help"), SO_NONE },
	SO_END_OF_OPTIONS
};
#endif //NOCMDLINE

//======== helper functions ====================================================
int getRandomPort()
{
	srand ((int)time (0));
	return 12000 + (rand()%500);
}

std::string Config::getPublicIP()
{
	SWBaseSocket::SWBaseError error;
	SWInetSocket mySocket;
	
	if( !mySocket.connect(80, REPO_SERVER, &error) )
		return "";

	char query[2048] = {0};
	sprintf(query, "GET /getpublicip/ HTTP/1.1\r\nHost: %s\r\n\r\n", REPO_SERVER);
	Logger::log(LOG_DEBUG, "Query to get public IP: %s\n", query);
	if( mySocket.sendmsg(query, &error) < 0 )
		return "";
	
	std::string retval = mySocket.recvmsg(250, &error);
	if( error != SWBaseSocket::ok )
		return "";
	Logger::log(LOG_DEBUG, "Response from public IP request :'%s'", retval.c_str() );
	
	HttpMsg msg( retval );
	retval = msg.getBody();
	//		printf("Response:'%s'\n", pubip);
	
	// disconnect
	mySocket.disconnect();
	return retval;
}

void showUsage()
{
	printf("\n" \
"Usage: rorserver [OPTIONS] <paramaters>\n" \
" Where [OPTIONS] and <parameters>\n" \
" -c (-config) <config file>   Loads the configuration from a file rather than from the commandline\n"
" -name <name>                 Name of the server, no spaces, only \n"
"                              [a-z,0-9,A-Z]\n"
" -terrain <mapname>           Map name (defaults to 'any')\n"
" -maxclients|speed <clients>  Maximum clients allowed \n"
" -lan|inet                    Private or public server (defaults to inet)\n"
"\n"
" -password <password>         Private server password\n"
" -ip <ip>                     Public IP address to register with.\n"
" -port <port>                 Port to use (defaults to random 12000-12500)\n"
" -verbosity {0-5}             Sets displayed log verbosity\n"
" -logverbosity {0-5}          Sets file log verbositylog verbosity\n"
"                              levels available to verbosity and logverbosity:\n"
"                                  0 = stack\n"
"                                  1 = debug\n"
"                                  2 = verbosity\n"
"                                  3 = info\n"
"                                  4 = warn\n"
"                                  5 = error\n" \
" -logfilename <server.log>    Sets the filename of the log\n" \
" -script <script.as>          server script to execute\n" \
" -webserver                   enables the built-in webserver\n" \
" -webserver-port <number>     sets up the port for the webserver, default is game port + 100\n" \
" -script <script.as>          server script to execute\n" \
" -version                     prints the server version numbers\n" \
" -fg                          starts the server in the foreground (background by default)\n" \
" -help                        Show this list\n");
}

//==============================================================================


Config Config::instance;

Config::Config():
	max_clients( 16 ),
	server_name( "" ),
	terrain_name( "any" ),
	ip_addr( "0.0.0.0" ),
	scriptname(""),
	listen_port( 0 ),
	server_mode( SERVER_AUTO ),
	print_stats(false),
	webserver_port( 0 ),
	foreground(false)
{
}

Config::~Config()
{
}

//! runs a check that all the required fields are present
bool Config::checkConfig()
{
	
	switch ( getServerMode() )
	{
	case SERVER_AUTO:
		Logger::log(LOG_INFO, "server started in automatic mode.");
		break;
	case SERVER_LAN:
		Logger::log(LOG_INFO, "server started in LAN mode.");
		break;
	case SERVER_INET:
		Logger::log(LOG_INFO, "server started in Internet mode.");
		break;
	}

	// settings required by INET mode
	if( getServerMode() != SERVER_LAN )
	{

        Logger::log( LOG_INFO, "Starting server in INET mode" );
	    if( getIPAddr() == "0.0.0.0" )
	    {
	        Logger::log( LOG_WARN, "no IP address has been specified, attempting to "
	                "detect.");
	        setIPAddr( getPublicIP() );
	        
	        if( getIPAddr().empty() )
	            Logger::log(LOG_ERROR, "could not get public IP automatically!");
	    }
	    
		if( getIPAddr().empty() )
		{
			Logger::log( LOG_ERROR, "IP adddress not specified.");
			return 0;
		}
		else
			Logger::log( LOG_INFO, "ip address: %s", getIPAddr().c_str() );
		
		Logger::log(LOG_WARN, "app. full load traffic: %ikbit/s upload and "
				"%ikbit/s download", 
				getMaxClients()*(getMaxClients()-1)*64, getMaxClients()*64);
		
		if( getServerName().empty() )
		{
			Logger::log( LOG_ERROR, "Server name not specified.");
			return 0;
		}
		else
			Logger::log( LOG_INFO, "servername: %s", getServerName().c_str() );		
	}
	if( !getListenPort() )
	{
		Logger::log( LOG_WARN, "No port supplied, randomly generating one");
		setListenPort( getRandomPort() );
	}

	if( getWebserverEnabled() && !getWebserverPort() )
	{
		Logger::log( LOG_WARN, "No Webserver port supplied, using listen port + 100: %d", getListenPort());
		setWebserverPort(getListenPort() + 100);
	}

	Logger::log( LOG_INFO, "port:       %d", getListenPort() );
	
	if( getTerrainName().empty() )
	{
		Logger::log( LOG_ERROR, "terrain not specified" );
		return 0;
	}
	else
		Logger::log( LOG_INFO, "terrain:    %s", getTerrainName().c_str() );
	
	if( getMaxClients() < 2 || getMaxClients() > 64 )
	{
		Logger::log( LOG_ERROR, "Max clients need to 2 or more, and 64 or less." );
		return 0;
	}
	else
		Logger::log( LOG_INFO, "maxclients: %d", getMaxClients());
	
	Logger::log( LOG_INFO, "server is%s password protected",
			getPublicPassword().empty() ? " NOT": "" );

	return getMaxClients() && getListenPort() && !getIPAddr().empty() && 
		!getServerName().empty() &&!getTerrainName().empty();
}

bool Config::fromArgs( int argc, char* argv[] )
{
#ifndef NOCMDLINE
	// parse arguments
	CSimpleOpt args(argc, argv, cmdline_options);
	while (args.Next()) {
		if (args.LastError() == SO_SUCCESS) {
			switch( args.OptionId() ) 
			{
			case OPT_IP:
				setIPAddr( args.OptionArg() );
				break;
			case OPT_NAME:
				setServerName( args.OptionArg() );
			break;
			case OPT_LOGFILENAME:
				Logger::setOutputFile(std::string(args.OptionArg()));
			break;
			case OPT_SCRIPTNAME:
				setScriptName(args.OptionArg());
			break;
			case OPT_TERRAIN:
				setTerrain( args.OptionArg() );
			break;
			case OPT_PASS:
				setPublicPass( args.OptionArg() );
			break;
			case OPT_VERSION:
				printf("Rigs of Rods Server\n");
				printf(" * using Protocol %s\n", RORNET_VERSION);
				printf(" * Revision %s\n", VERSION);
				printf(" * built on %s, %s\n", __DATE__, __TIME__);
#ifdef __GNUC__
				printf(" * built with gcc %d.%d.%d\n", __GNUC_MINOR__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif //__GNUC__
				exit(0);
			break;
			case OPT_PORT:
				setListenPort( atoi(args.OptionArg()) );
			break;
			case OPT_VERBOSITY:
				Logger::setLogLevel(LOGTYPE_DISPLAY, LogLevel(atoi(args.OptionArg())));
			break;
			case OPT_LOGVERBOSITY:
				Logger::setLogLevel(LOGTYPE_FILE, LogLevel(atoi(args.OptionArg())));
			break;
			case OPT_LAN:
				setServerMode( SERVER_LAN );
			break;
			case OPT_INET:
				setServerMode( SERVER_INET );
			break;
			case OPT_WEBSERVER:
				setWebserverEnabled(true);
			break;
			case OPT_WEBSERVER_PORT:
				setWebserverPort(atoi(args.OptionArg()));
			break;
			case OPT_MAXCLIENTS:
				setMaxClients( atoi(args.OptionArg()) );
			break;
			case OPT_FOREGROUND:
				setForeground(true);
			break;
			case OPT_CONFIGFILE:
				loadConfigFile(args.OptionArg());
			break;
			case OPT_HELP: 
			default:
				showUsage();
				return false;
			}
		}
	}
	if(getForeground() && !getWebserverEnabled() && !getPrintStats())
	{
		// add console overview printing when the webserver is not enabled
		setPrintStats(true);
	}

#endif //NOCMDLINE
	return true;
}

//! checks if a password has been set for server access
bool Config::isPublic() { return !getPublicPassword().empty(); }

//! getter function
//!@{
unsigned int       Config::getMaxClients()      { return instance.max_clients;     }
const std::string& Config::getServerName()      { return instance.server_name;     }
const std::string& Config::getTerrainName()     { return instance.terrain_name;    }
const std::string& Config::getPublicPassword()  { return instance.public_password; }
const std::string& Config::getIPAddr()          { return instance.ip_addr;         }
const std::string& Config::getScriptName()      { return instance.scriptname;      }
bool               Config::getEnableScripting() { return (instance.scriptname != ""); }
unsigned int       Config::getListenPort()      { return instance.listen_port;     }
ServerType         Config::getServerMode()      { return instance.server_mode;     }
bool               Config::getPrintStats()      { return instance.print_stats;     }
bool               Config::getWebserverEnabled(){ return instance.webserver_enabled; }
unsigned int       Config::getWebserverPort()   { return instance.webserver_port;     }
bool               Config::getForeground()      { return instance.foreground;     }
//!@}

//! setter functions
//!@{
bool Config::setScriptName(const std::string& name ) { 
	if( name.empty() ) return false;
	instance.scriptname = name;
 	return true;
}
bool Config::setMaxClients(unsigned int num) { 
	if( num < 2 || (getServerMode() == SERVER_INET) || num > 64 ) return false;
	instance.max_clients = num;
 	return true;
}
bool Config::setServerName( const std::string& name ) {
	if( name.empty() ) return false;
	instance.server_name = name;
	return true;
}
bool Config::setTerrain( const std::string& tern ) {
	if( tern.empty()) return false;
	instance.terrain_name = tern;
	return true;
}
bool Config::setPublicPass( const std::string& pub_pass ) {
	if(pub_pass.length() > 0 && pub_pass.size() < 250  &&  
			!SHA1FromString(instance.public_password, pub_pass))
	{
		Logger::log(LOG_ERROR, "could not generate server SHA1 password hash!");
		instance.public_password = "";
		return false;
	}
	Logger::log(LOG_DEBUG,"sha1(%s) = %s", pub_pass.c_str(), 
			instance.public_password.c_str());
	return true;
}

bool Config::setIPAddr( const std::string& ip ) {
	if( ip.empty() ) return false;
	instance.ip_addr = ip;
	return true;
}
bool Config::setListenPort( unsigned int port ) {
	instance.listen_port = port;
	return true;
}
void Config::setWebserverPort( unsigned int port ) {
	instance.webserver_port = port;
}
void Config::setWebserverEnabled(bool webserver) {
	instance.webserver_enabled = webserver;
}
bool Config::setServerMode( ServerType mode) {
	instance.server_mode = mode;
	return true;
}
void Config::setPrintStats(bool value)
{
	instance.print_stats = value;
}
void Config::setForeground(bool value) {
	instance.foreground = value;
}

void Config::loadConfigFile(const std::string& filename)
{
	rude::Config config;
	if(config.load(filename.c_str()))
	{
		if(config.exists("slots"))         setMaxClients(config.getIntValue       ("slots"));
		if(config.exists("name"))          setServerName(config.getStringValue    ("name"));
		if(config.exists("scriptname"))    setScriptName(config.getStringValue    ("scriptname"));
		if(config.exists("terrain"))       setTerrain   (config.getStringValue    ("terrain"));
		if(config.exists("terrain"))       setTerrain   (config.getStringValue    ("terrain"));
		if(config.exists("password"))      setPublicPass(config.getStringValue    ("password"));
		if(config.exists("ip"))            setIPAddr    (config.getStringValue    ("ip"));
		if(config.exists("port"))          setListenPort(config.getIntValue       ("port"));
		if(config.exists("mode"))          setServerMode(config.getStringValue    ("mode") == "inet"?SERVER_INET:SERVER_LAN);
		
		if(config.exists("printstats"))    setPrintStats(config.getBoolValue      ("printstats"));
		if(config.exists("webserver"))     setWebserverEnabled(config.getBoolValue("webserver"));
		if(config.exists("webserverport")) setWebserverPort(config.getIntValue    ("webserverport"));
		if(config.exists("foreground"))    setForeground(config.getBoolValue      ("foreground"));

		if(config.exists("verbosity"))     Logger::setLogLevel(LOGTYPE_DISPLAY,   (LogLevel)config.getIntValue("verbosity"));
		if(config.exists("logverbosity"))  Logger::setLogLevel(LOGTYPE_FILE,      (LogLevel)config.getIntValue("logverbosity"));
	} else
	{
		Logger::log(LOG_ERROR, "could not load config file %s : %s", filename.c_str(), config.getError());
	}
}
//!@}
