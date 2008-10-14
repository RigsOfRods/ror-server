#include "config.h"

// simpleopt by http://code.jellycan.com/simpleopt/
// license: MIT
#include "SimpleOpt.h"
#include "SocketW.h"
#include "rornet.h"
#include "HttpMsg.h"
#include "logger.h"
#include "sha1_util.h"

#include <cmath>
#include <fstream>

#ifdef __WIN32__
#include <windows.h>
#include <time.h>
#endif

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
    OPT_AUTO,
	OPT_VERBOSITY,
	OPT_LOGVERBOSITY,
	OPT_PASS,
	OPT_INET,
	OPT_RCONPASS,
	OPT_GUI,
	OPT_UPSPEED,
	OPT_LOGFILENAME,
	OPT_BANNED_USERS,
	OPT_BANNED_TRUCKS,
	OPT_BANNED_IP
};

// option array
static CSimpleOpt::SOption cmdline_options[] = {
	{ OPT_IP,   ("-ip"),   SO_REQ_SEP },
	{ OPT_PORT, ("-port"), SO_REQ_SEP },
	{ OPT_NAME, ("-name"), SO_REQ_SEP },
	{ OPT_PASS, ("-password"), SO_REQ_SEP },
	{ OPT_RCONPASS, ("-rconpassword"), SO_REQ_SEP },
	{ OPT_TERRAIN, ("-terrain"), SO_REQ_SEP },
	{ OPT_MAXCLIENTS, ("-maxclients"), SO_REQ_SEP },
	{ OPT_UPSPEED, ("-speed"), SO_REQ_SEP },
	{ OPT_LAN, ("-lan"), SO_NONE },
	{ OPT_INET, ("-inet"), SO_NONE },
    { OPT_AUTO, ("-auto"), SO_NONE },
	{ OPT_VERBOSITY, ("-verbosity"), SO_REQ_SEP },
	{ OPT_LOGVERBOSITY, ("-logverbosity"), SO_REQ_SEP },
	{ OPT_LOGFILENAME, ("-logfilename"), SO_REQ_SEP },
    { OPT_BANNED_USERS, ( "-banned_user_file"), SO_REQ_SEP },
    { OPT_BANNED_TRUCKS, ( "-banned_truck_file"), SO_REQ_SEP },
    { OPT_BANNED_IP, ( "-banned_ip_file"), SO_REQ_SEP },
	{ OPT_GUI, ("-gui"), SO_NONE },
	{ OPT_HELP,  ("--help"), SO_NONE },
	SO_END_OF_OPTIONS
};

//======== helper functions ====================================================
int getRandomPort()
{
	srand ((int)time (0));
	return 12000 + (rand()%500);
}

static std::string getPublicIP()
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
" REQUIRED:\n" \
" -name <name>                 Name of the server, no spaces, only \n"
"                              [a-z,0-9,A-Z]\n"
" -terrain <mapname>           Map name (defaults to 'any')\n"
"                              Use maxclients OR speed, not both\n"
" -maxclients|speed <clients>  Maximum clients allowed \n"
" -speed <upload in kbps>      or maximum upload speed (in kilobits)\n"
"\n"
" SERVER MODES (defaults to auto):\n"
" -lan                         Starts a server without attempting to register\n"
" -inet                        Starts a server and registers with the central\n"
"                              server list\n"
" -auto                        Attempts to start in inet mode, if this fails\n"
"                              Attempts to start in lan mode\n"
"\n"
" OPTIONAL:\n"
" -password <password>         Private server password\n"
" -rconpassword <password>     Set admin password. This is required for RCON to\n"
"                              work. Otherwise RCON is disabled.\n"
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
" -banned_user_file <filename> Reads a list of regular expressed used for\n"
"                              identifying banned members and invalid usernames\n"
" -banned_truck_file <filename> Reads a list of regular expressed used for\n"
"                              identifying invalid truck files\n"
" -banned_ip_file <filename>   Reads a list of regular expressed used for\n"
"                              identifying banned IP addresses\n"
" -help                        Show this list\n");
}

//==============================================================================


Config Config::instance;

Config::Config():
max_clients( 16 ),
server_name( "" ),
terrain_name( "any" ),
public_password( "" ),
admin_password( "" ),
ip_addr( "" ),
listen_port( 0 ),
server_mode( SERVER_AUTO )
{
}

Config::~Config()
{
}

//! runs a check that all the required fields are present
bool Config::checkConfig()
{
	switch ( ServerMode() )
	{
	case SERVER_AUTO:
		Logger::log(LOG_INFO, "server started in automatic mode.");
		break;
	case SERVER_INET:
		Logger::log(LOG_INFO, "server started in Internet mode.");
		break;
    case SERVER_LAN:
        Logger::log(LOG_INFO, "server started in LAN mode.");
        break;
	}

	// settings required by INET mode
	if( checkINETConfig() )
	    Logger::log(LOG_INFO, "server contains a valid Internet configuration.");
	else if( ServerMode() == SERVER_INET)
    {
        return false;
    }

	// server must contain a valid LAN Config
	if( checkLANConfig() )
	    Logger::log(LOG_INFO, "Server contains a valid LAN configuration.");
	else
	    return false;

	Logger::log( LOG_INFO, "server is%s password protected",
			PublicPassword().empty() ? " NOT": "" );

	Logger::log( LOG_INFO, "admin password %s admin console",
			AdminPassword().empty() ? "not set, disabling":
			"is set, enabling" );

	return true;

}

bool Config::checkINETConfig()
{
    if( ServerMode() == SERVER_LAN ) return false;
    return validIPAddr() && validINETClients() && validServerName();
}

bool Config::checkLANConfig()
{
    return validPort() && validTerrn() && validNumClients();
}

bool Config::validIPAddr()
{
    if( IPAddr().empty() )
    {
        Logger::log( LOG_WARN, "no IP address has been specified, attempting "
                "auto detection...");
        IPAddr( getPublicIP() );

        if( IPAddr().empty() )
        {
            Logger::log(LOG_ERROR, "IP Address auto detection failed!");
            return false;
        }
    }

    Logger::log( LOG_INFO, "ip address: %s", IPAddr().c_str() );
    return true;
}
bool Config::validINETClients()
{
    if( MaxClients() > 16 )
    {
        Logger::log( LOG_ERROR, "max clients is set to greater than 16,"
                "reseting to 16");
        MaxClients( 16 );
    }
    Logger::log(LOG_WARN, "app. full load traffic: %ikbit/s upload and "
            "%ikbit/s download",
            MaxClients()*(MaxClients()-1)*64, MaxClients()*64);
    return true;

}
bool Config::validServerName()
{
    if( ServerName().empty() )
    {
        Logger::log( LOG_ERROR, "Server name not specified.");
        return false;
    }

    Logger::log( LOG_INFO, "servername: %s", ServerName().c_str() );
    return true;

}

bool Config::validPort()
{
    if( !ListenPort() )
    {
        Logger::log( LOG_WARN, "No port supplied, randomly generating one");
        ListenPort( getRandomPort() );
    }

    Logger::log( LOG_INFO, "port:       %d", ListenPort() );
    return true;
}
bool Config::validTerrn()
{
    if( TerrainName().empty() )
    {
        Logger::log( LOG_ERROR, "terrain not specified" );
        return false;
    }

    Logger::log( LOG_INFO, "terrain:    %s", TerrainName().c_str() );
    return true;
}
bool Config::validNumClients()
{
    if( MaxClients() < 2 || MaxClients() > 64 )
    {
        Logger::log( LOG_ERROR, "Required number of clients is between 2 and 64" );
        return false;
    }

    Logger::log( LOG_INFO, "maxclients: %d", MaxClients());
    return true;

}

bool Config::fromArgs( int argc, char* argv[] )
{
	// parse arguments
	CSimpleOpt args(argc, argv, cmdline_options);
	while (args.Next() && args.LastError() == SO_SUCCESS) {
        switch( args.OptionId() )
        {
        case OPT_IP:
            IPAddr( args.OptionArg() );
            break;
        case OPT_NAME:
            ServerName( args.OptionArg() );
        break;
        case OPT_LOGFILENAME:
            Logger::setOutputFile(std::string(args.OptionArg()));
        break;
        case OPT_TERRAIN:
            Terrain( args.OptionArg() );
        break;
        case OPT_PASS:
            PublicPass( args.OptionArg() );
        break;
        case OPT_UPSPEED:
            MaxClients( (int)floor( (1 + sqrt(  ( float ) 1 - 4 * ( - (atoi(args.OptionArg())/64))))/2) );
        break;
        case OPT_RCONPASS:
            AdminPass( args.OptionArg() );
        break;
        case OPT_PORT:
            ListenPort( atoi(args.OptionArg()) );
        break;
        case OPT_VERBOSITY:
            Logger::setLogLevel(LOGTYPE_DISPLAY, LogLevel(atoi(args.OptionArg())));
        break;
        case OPT_LOGVERBOSITY:
            Logger::setLogLevel(LOGTYPE_FILE, LogLevel(atoi(args.OptionArg())));
        break;
        case OPT_LAN:
            ServerMode( SERVER_LAN );
        break;
        case OPT_INET:
            ServerMode( SERVER_INET );
        break;
        case OPT_AUTO:
            ServerMode( SERVER_AUTO );
        break;
        case OPT_MAXCLIENTS:
            MaxClients( atoi(args.OptionArg()) );
        break;

        case  OPT_BANNED_USERS:
            loadBannedUsernames( std::string( args.OptionArg() ) );
        break;
        case  OPT_BANNED_TRUCKS:
            loadBannedTrucks( std::string( args.OptionArg() ) );
        break;
        case  OPT_BANNED_IP:
            loadBannedIPaddr( std::string( args.OptionArg() ) );
        break;

        case OPT_HELP:
        default:
            showUsage();
            return false;
        }
	}
	return true;
}

//! checks if a password has been set for server access
bool Config::isPublic() { return !PublicPassword().empty(); }
//! checks if the admin password is set, and the admin console is enabled.
bool Config::hasAdmin() { return !AdminPassword().empty(); }

//! getter function
//!@{
unsigned int       Config::MaxClients()     { return instance.max_clients;     }
const std::string& Config::ServerName()     { return instance.server_name;     }
const std::string& Config::TerrainName()    { return instance.terrain_name;    }
const std::string& Config::PublicPassword() { return instance.public_password; }
const std::string& Config::AdminPassword()  { return instance.admin_password;  }
const std::string& Config::IPAddr()         { return instance.ip_addr;         }
unsigned int       Config::ListenPort()     { return instance.listen_port;     }
ServerType         Config::ServerMode()     { return instance.server_mode;     }
//!@}

//! setter functions
//!@{
bool Config::MaxClients(unsigned int num) {
	if( num < 2 || (ServerMode() == SERVER_INET) || num > 64 ) return false;
	instance.max_clients = num;
 	return true;
}
bool Config::ServerName( const std::string& name ) {
	if( name.empty() ) return false;
	instance.server_name = name;
	return true;
}
bool Config::Terrain( const std::string& tern ) {
	if( tern.empty()) return false;
	instance.terrain_name = tern;
	return true;
}
bool Config::PublicPass( const std::string& pub_pass ) {
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

bool Config::AdminPass( const std::string& admin_pass ) {
	if(admin_pass.length() > 0 && admin_pass.length() < 250  &&
			!SHA1FromString(instance.admin_password, admin_pass))
	{
		Logger::log(LOG_ERROR, "could not generate server SHA1 password hash!");
		instance.admin_password = "";
		return false;
	}
	Logger::log(LOG_DEBUG,"sha1(%s) = %s", admin_pass.c_str(),
			instance.admin_password.c_str());
	return true;
}

bool Config::IPAddr( const std::string& ip ) {
	if( ip.empty() ) return false;
	instance.ip_addr = ip;
	return true;
}
bool Config::ListenPort( unsigned int port ) {
	// ports less than 1000 are reserved, so avoid that hassle
	if( port <= 1000 ) return false;
	instance.listen_port = port;
	return true;
}
bool Config::ServerMode( ServerType mode) {
	instance.server_mode = mode;
	return true;
}
//!@}


bool Config::bannedUsername( const std::string& name ) {
    return bannedRegex( name, instance.bannedNames );
}
bool Config::bannedTruck( const std::string& truck_file ) {
    return bannedRegex( truck_file, instance.bannedTrucks );
}
bool Config::bannedIP( const std::string& ip_addr ) {
    return bannedRegex( ip_addr, instance.bannedIPs );
}

void Config::loadBannedUsernames( const std::string& filename ) {
    loadRegexFromFile( filename, instance.bannedNames );
}
void Config::loadBannedTrucks( const std::string& filename ) {
    loadRegexFromFile( filename, instance.bannedTrucks );
}
void Config::loadBannedIPaddr( const std::string& filename ) {
    loadRegexFromFile( filename, instance.bannedIPs );
}


bool Config::bannedRegex( const std::string& checkVal,
        const std::vector<boost::regex>& regexs)
{
    bool match = false;
    Logger::log( LOG_INFO,"checking %s against %d regular expressions",
            checkVal.c_str(), regexs.size() );
    for( std::vector<boost::regex>::const_iterator
            current( regexs.begin() ), end( regexs.end() );
        (current != end) && !match; current++ )
    {
        match = regex_match( checkVal, *current );
    }
    return match;
}


void Config::loadRegexFromFile(  const std::string& filename,
        std::vector<boost::regex>& regexs)
{
#ifdef BANNING_TEST
    if( filename.empty() )
    {
        regexs.push_back( boost::regex(".*") ); // block everything as a test
        Logger::log( LOG_INFO, "no valid names provided, allowing all" );
        return;
    }
#undef BANNING_TEST
#endif

    std::ifstream file( filename.c_str() );
    std::string expression;
    if( file.is_open() )
    {
        while ( !file.eof() )
        {
            getline( file,expression );
            Logger::log( LOG_INFO, "got valid regular expression: %s",
                    expression.c_str() );

            regexs.push_back( boost::regex( expression ) );
        }
        file.close();
    }
}
