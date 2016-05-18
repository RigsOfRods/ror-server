#include "config.h"

#include "logger.h"
#include "sequencer.h"
#include "sha1_util.h"
#include "utils.h"
#include "messaging.h"

// simpleopt by http://code.jellycan.com/simpleopt/
// license: MIT
#include "SimpleOpt.h"
#include <rudeconfig/config.h>
#include <cmath>
#ifdef __GNUC__
#include <unistd.h>
#include <stdlib.h>
#endif

// ============================== Constants ===================================

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
	OPT_CONFIGFILE,
	OPT_RESDIR,
	OPT_AUTHFILE,
	OPT_MOTDFILE,
	OPT_RULESFILE,
	OPT_VEHICLELIMIT,
	OPT_OWNER,
	OPT_WEBSITE,
	OPT_IRC,
	OPT_VOIP 
};

// option array
static CSimpleOpt::SOption cmdline_options[] = {
	{ OPT_IP,             ((char *)"-ip"),            SO_REQ_SEP },
	{ OPT_PORT,           ((char *)"-port"),          SO_REQ_SEP },
	{ OPT_NAME,           ((char *)"-name"),          SO_REQ_SEP },
	{ OPT_PASS,           ((char *)"-password"),      SO_REQ_SEP },
	{ OPT_TERRAIN,        ((char *)"-terrain"),       SO_REQ_SEP },
	{ OPT_MAXCLIENTS,     ((char *)"-maxclients"),    SO_REQ_SEP },
	{ OPT_LAN,            ((char *)"-lan"),           SO_NONE    },
	{ OPT_INET,           ((char *)"-inet"),          SO_NONE    },
	{ OPT_VERBOSITY,      ((char *)"-verbosity"),     SO_REQ_SEP },
	{ OPT_LOGVERBOSITY,   ((char *)"-logverbosity"),  SO_REQ_SEP },
	{ OPT_LOGFILENAME,    ((char *)"-logfilename"),   SO_REQ_SEP },
	{ OPT_SCRIPTNAME,     ((char *)"-script"),        SO_REQ_SEP },
	{ OPT_WEBSERVER,      ((char *)"-webserver"),     SO_NONE    },
	{ OPT_WEBSERVER_PORT, ((char *)"-webserverport"), SO_REQ_SEP },
	{ OPT_VERSION,        ((char *)"-version"),       SO_NONE    },
	{ OPT_HELP,           ((char *)"-?"),             SO_NONE    },
	{ OPT_HELP,           ((char *)"-h"),             SO_NONE    },
	{ OPT_HELP,           ((char *)"-help"),          SO_NONE    },
	{ OPT_HELP,           ((char *)"--help"),         SO_NONE    },
	{ OPT_HELP,           ((char *)"/\?"),            SO_NONE    },
	{ OPT_HELP,           ((char *)"/help"),          SO_NONE    },
	{ OPT_HELP,           ((char *)"/h"),             SO_NONE    },
	{ OPT_FOREGROUND,     ((char *)"-fg"),            SO_NONE    },
	{ OPT_CONFIGFILE,     ((char *)"-c"),             SO_REQ_SEP },
	{ OPT_CONFIGFILE,     ((char *)"-config"),        SO_REQ_SEP },
	{ OPT_RESDIR,         ((char *)"-resdir"),        SO_REQ_SEP },
	{ OPT_AUTHFILE,       ((char *)"-authfile"),      SO_REQ_SEP },
	{ OPT_MOTDFILE,       ((char *)"-motdfile"),      SO_REQ_SEP },
	{ OPT_RULESFILE,      ((char *)"-rulesfile"),     SO_REQ_SEP },
	{ OPT_VEHICLELIMIT,   ((char *)"-vehiclelimit"),  SO_REQ_SEP },
	{ OPT_OWNER,          ((char *)"-owner"),         SO_REQ_SEP },
	{ OPT_WEBSITE,        ((char *)"-website"),       SO_REQ_SEP },
	{ OPT_IRC,            ((char *)"-irc"),           SO_REQ_SEP },
	{ OPT_VOIP,           ((char *)"-voip"),          SO_REQ_SEP },
	SO_END_OF_OPTIONS
};
#endif //NOCMDLINE

// ============================== Variables ===================================

static std::string s_server_name;
static std::string s_terrain_name("any");
static std::string s_public_password;
static std::string s_ip_addr("0.0.0.0");
static std::string s_scriptname;
static std::string s_authfile("admins.txt");
static std::string s_motdfile("motd.txt");
static std::string s_rulesfile("rules.txt");
static std::string s_owner;
static std::string s_website;
static std::string s_irc;
static std::string s_voip;

#ifdef _WIN32
static std::string s_resourcedir;
#else // _WIN32
static std::string s_resourcedir("/usr/share/rorserver/"); // trailing slash important
#endif // _WIN32

static unsigned int s_max_vehicles(20);
static unsigned int s_webserver_port(0);
static unsigned int s_listen_port(0);
static unsigned int s_max_clients(16);

static bool s_print_stats(false);
static bool s_webserver_enabled(false);
static bool s_foreground(false);

static ServerType s_server_mode(SERVER_AUTO);

// ============================== Functions ===================================

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
" -resdir <path>               sets the path to the resource directory\n" \
" -authfile <server.auth>      Sets the filename of the file that contains authorization info\n" \
" -motdfile <server.motd>      Sets the filename of the file that contains the message of the day\n" \
" -rulesfile <server.rules>    Sets the filename of the file that contains rules for this server\n" \
" -vehiclelimit {0-...}        Sets the maximum number of vehicles that a user is allowed to have\n" \
" -owner <name|organisation>   Sets the owner of this server (for the !owner command) (optional)\n" \
" -website <URL>               Sets the website of this server (for the !website command) (optional)\n" \
" -irc <URL>                   Sets the IRC url for this server (for the !irc command) (optional)\n" \
" -voip <URL>                  Sets the voip url for this server (for the !voip command) (optional)\n" \
" -help                        Show this list\n");
}

namespace Config
{

//! runs a check that all the required fields are present
bool checkConfig()
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
	        setIPAddr( Messaging::retrievePublicIpFromServer() );
	        
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
		setListenPort( Utils::generateRandomPortNumber() );
	}

	if( getWebserverEnabled() && !getWebserverPort() )
	{
		Logger::log( LOG_WARN, "No Webserver port supplied, using listen port + 100: %d", getListenPort() + 100);
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
		
	if( getAuthFile().empty() )
	{
		Logger::log( LOG_ERROR, "Authorizations file not specified. Using default (admins.txt)" );
		setAuthFile("admins.txt");
	}

	if( getMOTDFile().empty() )
	{
		Logger::log( LOG_ERROR, "MOTD file not specified. Using default (motd.txt)." );
		setMOTDFile("motd.txt");
	}

	if( getMaxVehicles()<1 )
	{
		Logger::log( LOG_ERROR, "The vehicle-limit cannot be less than 1!" );
		return 0;
	}

	Logger::log( LOG_INFO, "server is%s password protected",
			getPublicPassword().empty() ? " NOT": "" );

	return getMaxClients() && getListenPort() && !getIPAddr().empty() && 
			!getTerrainName().empty();
}

bool fromArgs( int argc, char* argv[] )
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
			case OPT_RESDIR:
				setResourceDir(args.OptionArg());
			break;
			case OPT_AUTHFILE:
				setAuthFile(args.OptionArg());
			break;
			case OPT_MOTDFILE:
				setMOTDFile(args.OptionArg());
			break;
			case OPT_RULESFILE:
				setRulesFile(args.OptionArg());
			break;
			case OPT_VEHICLELIMIT:
				setMaxVehicles( atoi(args.OptionArg()) );
			break;
			case OPT_OWNER:
				setOwner(args.OptionArg());
			break;
			case OPT_WEBSITE:
				setWebsite(args.OptionArg());
			break;
			case OPT_IRC:
				setIRC(args.OptionArg());
			break;
			case OPT_VOIP:
				setVoIP(args.OptionArg());
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

bool isPublic() { return !getPublicPassword().empty(); }

unsigned int       getMaxClients()      { return s_max_clients;        }
const std::string& getServerName()      { return s_server_name;        }
const std::string& getTerrainName()     { return s_terrain_name;       }
const std::string& getPublicPassword()  { return s_public_password;    }
const std::string& getIPAddr()          { return s_ip_addr;            }
const std::string& getScriptName()      { return s_scriptname;         }
bool               getEnableScripting() { return (s_scriptname != ""); }
unsigned int       getListenPort()      { return s_listen_port;        }
ServerType         getServerMode()      { return s_server_mode;        }
bool               getPrintStats()      { return s_print_stats;        }
bool               getWebserverEnabled(){ return s_webserver_enabled;  }
unsigned int       getWebserverPort()   { return s_webserver_port;     }
bool               getForeground()      { return s_foreground;         }
const std::string& getResourceDir()     { return s_resourcedir;        }
const std::string& getAuthFile()        { return s_authfile;           }
const std::string& getMOTDFile()        { return s_motdfile;           }
const std::string& getRulesFile()       { return s_rulesfile;          }
unsigned int       getMaxVehicles()     { return s_max_vehicles;       }
const std::string& getOwner()           { return s_owner;              }
const std::string& getWebsite()         { return s_website;            }
const std::string& getIRC()             { return s_irc;                }
const std::string& getVoIP()            { return s_voip;               }


bool setScriptName(const std::string& name )
{ 
	if( name.empty() ) return false;
	s_scriptname = name;
	return true;
}

bool setMaxClients(unsigned int num)
{
	if( num < 2 || num > 64 ) return false;
	s_max_clients = num;
	return true;
}

bool setServerName( const std::string& name )
{
	if( name.empty() ) return false;
	s_server_name = name;
	return true;
}

bool setTerrain( const std::string& tern )
{
	if( tern.empty()) return false;
	s_terrain_name = tern;
	return true;
}

bool setPublicPass( const std::string& pub_pass )
{
	if(pub_pass.length() > 0 && pub_pass.size() < 250  &&  
			!SHA1FromString(s_public_password, pub_pass))
	{
		Logger::log(LOG_ERROR, "could not generate server SHA1 password hash!");
		s_public_password = "";
		return false;
	}
	Logger::log(LOG_DEBUG,"sha1(%s) = %s", pub_pass.c_str(), 
			s_public_password.c_str());
	return true;
}

bool setIPAddr( const std::string& ip )
{
	if( ip.empty() ) return false;
	s_ip_addr = ip;
	return true;
}

bool setListenPort( unsigned int port )
{
	s_listen_port = port;
	return true;
}

void setWebserverPort( unsigned int port )  { s_webserver_port = port;         }
void setWebserverEnabled(bool webserver)    { s_webserver_enabled = webserver; }
void setPrintStats(bool value)              { s_print_stats = value;           }
void setAuthFile(const std::string& file)   { s_authfile = file;               }
void setMOTDFile(const std::string& file)   { s_motdfile = file;               }
void setRulesFile(const std::string& file)  { s_rulesfile = file;              }
void setMaxVehicles(unsigned int num)       { s_max_vehicles = num;            }
void setOwner(const std::string& owner)     { s_owner = owner;                 }
void setForeground(bool value)              { s_foreground = value;            }
void setWebsite(const std::string& website) { s_website = website;             }
void setIRC(const std::string& irc)         { s_irc = irc;                     }
void setVoIP(const std::string& voip)       { s_voip = voip;                   }

bool setServerMode( ServerType mode)
{
	s_server_mode = mode;
	return true;
}

void setResourceDir(std::string dir)
{
	if(dir.length()>0 && dir.substr(dir.length()-1)!="/")
		dir += "/";
	s_resourcedir = dir;
}

void loadConfigFile(const std::string& filename)
{
	Logger::log(LOG_INFO, "loading config file %s ...", filename.c_str());
	rude::Config config;
	if(config.load(filename.c_str()))
	{
		if(config.exists("baseconfig"))    loadConfigFile(config.getStringValue   ("baseconfig"));
		if(config.exists("slots"))         setMaxClients(config.getIntValue       ("slots"));
		if(config.exists("name"))          setServerName(config.getStringValue    ("name"));
		if(config.exists("scriptname"))    setScriptName(config.getStringValue    ("scriptname"));
		if(config.exists("terrain"))       setTerrain   (config.getStringValue    ("terrain"));
		if(config.exists("password"))      setPublicPass(config.getStringValue    ("password"));
		if(config.exists("ip"))            setIPAddr    (config.getStringValue    ("ip"));
		if(config.exists("port"))          setListenPort(config.getIntValue       ("port"));
		if(config.exists("mode"))          setServerMode((std::string(config.getStringValue("mode")) == std::string("inet"))?SERVER_INET:SERVER_LAN);

		if(config.exists("printstats"))    setPrintStats(config.getBoolValue      ("printstats"));
		if(config.exists("webserver"))     setWebserverEnabled(config.getBoolValue("webserver"));
		if(config.exists("webserverport")) setWebserverPort(config.getIntValue    ("webserverport"));
		if(config.exists("foreground"))    setForeground(config.getBoolValue      ("foreground"));

		if(config.exists("verbosity"))     Logger::setLogLevel(LOGTYPE_DISPLAY,   (LogLevel)config.getIntValue("verbosity"));
		if(config.exists("logverbosity"))  Logger::setLogLevel(LOGTYPE_FILE,      (LogLevel)config.getIntValue("logverbosity"));
		if(config.exists("resdir"))        setResourceDir(config.getStringValue   ("resdir"));
		if(config.exists("logfilename"))   Logger::setOutputFile(config.getStringValue("logfilename"));
		if(config.exists("authfile"))      setAuthFile(config.getStringValue      ("authfile"));
		if(config.exists("motdfile"))      setMOTDFile(config.getStringValue      ("motdfile"));
		if(config.exists("rulesfile"))     setRulesFile(config.getStringValue     ("rulesfile"));
		if(config.exists("vehiclelimit"))  setMaxVehicles(config.getIntValue      ("vehiclelimit"));
		if(config.exists("owner"))         setOwner(config.getStringValue         ("owner"));
		if(config.exists("website"))       setWebsite(config.getStringValue       ("website"));
		if(config.exists("irc"))           setIRC(config.getStringValue           ("irc"));
		if(config.exists("voip"))          setVoIP(config.getStringValue          ("voip"));
	} else
	{
		Logger::log(LOG_ERROR, "could not load config file %s : %s", filename.c_str(), config.getError());
	}
}

} //namespace Config

