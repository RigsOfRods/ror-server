/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include "logger.h"
#include "sequencer.h"
#include "sha1_util.h"
#include "utils.h"

#include <rudeconfig/config.h>
#include <cmath>
#ifdef __GNUC__
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef _WIN32
#   define RESOURCE_DIR ""
#else // _WIN32 ~ trailing slash important
#   define RESOURCE_DIR "/usr/share/rorserver/"
#endif // _WIN32

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
static std::string s_serverlist_host("multiplayer.rigsofrods.org");
static std::string s_resourcedir(RESOURCE_DIR);

static unsigned int s_max_vehicles(20);
static unsigned int s_webserver_port(0);
static unsigned int s_listen_port(0);
static unsigned int s_max_clients(16);
static unsigned int s_heartbeat_retry_count(5);
static unsigned int s_heatbeat_retry_seconds(15);

static bool s_print_stats(false);
static bool s_webserver_enabled(false);
static bool s_foreground(false);
static bool s_show_version(false);
static bool s_show_help(false);

static ServerType s_server_mode(SERVER_AUTO);

// ============================== Functions ===================================

namespace Config
{

void ShowHelp()
{
    printf(
        " -c (-config) <config file>   Loads the configuration from a file rather than from the commandline\n"
        "Usage: rorserver [OPTIONS]\n"
        "[OPTIONS] can be in Un*x `--help` or windows `/help` notation\n"
        "\n"
        " ~config-file (-c) <JSON file> Loads the configuration from a file\n"
        " ~name <name>                 Name of the server, no spaces, only\n"
        "                              [a-z,0-9,A-Z]\n"
        " ~terrain <mapname>           Map name (defaults to 'any')\n"
        " ~max-clients|speed <clients> Maximum clients allowed\n"
        " ~lan|inet                    Private or public server (defaults to inet)\n"
        "\n"
        " ~password <password>         Private server password\n"
        " ~ip <ip>                     Public IP address to register with.\n"
        " ~port <port>                 Port to use (defaults to random 12000-12500)\n"
        " ~verbosity {0-5}             Sets displayed log verbosity\n"
        " ~log-verbosity {0-5}         Sets file log verbositylog verbosity\n"
        "                              levels available to verbosity and logverbosity:\n"
        "                                  0 = stack\n"
        "                                  1 = debug\n"
        "                                  2 = verbosity\n"
        "                                  3 = info\n"
        "                                  4 = warn\n"
        "                                  5 = error\n"
        " ~log-file <server.log>       Sets the filename of the log\n"
        " ~script-file <script.as>     server script to execute\n"
        " ~use-webserver               enables the built-in webserver\n"
        " ~webserver-port <number>     sets up the port for the webserver, default is game port + 100\n"
        " ~version                     prints the server version numbers\n"
        " ~fg                          starts the server in the foreground (background by default)\n"
        " ~resource-dir <path>         sets the path to the resource directory\n"
        " ~auth-file <server.auth>     Sets the filename of the file that contains authorization info\n"
        " ~motdf-ile <server.motd>     Sets the filename of the file that contains the message of the day\n"
        " ~rules-file <server.rules>   Sets the filename of the file that contains rules for this server\n"
        " ~vehicle-limit {0-...}       Sets the maximum number of vehicles that a user is allowed to have\n"
        " ~owner <name|organisation>   Sets the owner of this server (for the !owner command) (optional)\n"
        " ~website <URL>               Sets the website of this server (for the !website command) (optional)\n"
        " ~irc <URL>                   Sets the IRC url for this server (for the !irc command) (optional)\n"
        " ~voip <URL>                  Sets the voip url for this server (for the !voip command) (optional)\n"
        " ~help                        Show this list\n");
}

void ShowVersion()
{
    printf("Rigs of Rods Server\n");
    printf(" * using Protocol %s\n", RORNET_VERSION);
    printf(" * built on %s, %s\n", __DATE__, __TIME__);
#ifdef __GNUC__
    printf(" * built with gcc %d.%d.%d\n", __GNUC_MINOR__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif //__GNUC__
}

//! runs a check that all the required fields are present
bool checkConfig()
{
    switch ( getServerMode() )
    {
    case SERVER_AUTO:
        Logger::Log(LOG_INFO, "server started in automatic mode.");
        break;
    case SERVER_LAN:
        Logger::Log(LOG_INFO, "server started in LAN mode.");
        break;
    case SERVER_INET:
        Logger::Log(LOG_INFO, "server started in Internet mode.");
        break;
    }

    if( !getListenPort() )
    {
        Logger::Log( LOG_WARN, "No port supplied, randomly generating one");
        setListenPort( Utils::generateRandomPortNumber() );
    }

    if( getWebserverEnabled() && !getWebserverPort() )
    {
        Logger::Log( LOG_WARN, "No Webserver port supplied, using listen port + 100: %d", getListenPort() + 100);
        setWebserverPort(getListenPort() + 100);
    }

    Logger::Log( LOG_INFO, "port:       %d", getListenPort() );
    
    if( getTerrainName().empty() )
    {
        Logger::Log( LOG_ERROR, "terrain not specified" );
        return 0;
    }
    else
        Logger::Log( LOG_INFO, "terrain:    %s", getTerrainName().c_str() );
    
    if( getMaxClients() < 2 || getMaxClients() > 64 )
    {
        Logger::Log( LOG_ERROR, "Max clients need to 2 or more, and 64 or less." );
        return 0;
    }
    else
        Logger::Log( LOG_INFO, "maxclients: %d", getMaxClients());
        
    if( getAuthFile().empty() )
    {
        Logger::Log( LOG_ERROR, "Authorizations file not specified. Using default (admins.txt)" );
        setAuthFile("admins.txt");
    }

    if( getMOTDFile().empty() )
    {
        Logger::Log( LOG_ERROR, "MOTD file not specified. Using default (motd.txt)." );
        setMOTDFile("motd.txt");
    }

    if( getMaxVehicles()<1 )
    {
        Logger::Log( LOG_ERROR, "The vehicle-limit cannot be less than 1!" );
        return 0;
    }

    Logger::Log( LOG_INFO, "server is%s password protected",
            getPublicPassword().empty() ? " NOT": "" );

    return getMaxClients() && getListenPort() && !getIPAddr().empty() && 
            !getTerrainName().empty();
}

#define HANDLE_ARG_VALUE(_NAME_, _BLOCK_)           \
{                                                   \
    if(strcmp(arg, _NAME_) == 0)                    \
    {                                               \
        if (pos + 1 < argc)                         \
        {                                           \
            const char* value = argv[pos + 1];      \
            { _BLOCK_ }                             \
            pos += 2;                               \
            continue;                               \
        }                                           \
        else                                        \
        {                                           \
            Logger::Log(LOG_WARN,                   \
                "Command line error: argument `%s`" \
                "at position %d without value",     \
                arg, pos);                          \
            return false;                           \
        }                                           \
    }                                               \
}

#define HANDLE_ARG_FLAG(_NAME_, _BLOCK_)            \
{                                                   \
    if(strcmp(arg, _NAME_) == 0)                    \
    {                                               \
        _BLOCK_                                     \
        pos += 1;                                   \
        continue;                                   \
    }                                               \
}

bool ProcessArgs( int argc, char* argv[] )
{
#ifndef NOCMDLINE
    int pos = 1;
    const char* config_file = nullptr;
    while (pos < argc)
    {
        // Cut off the leading `-`, `--` or `/` (windows)
        char* arg = argv[pos];
        if ((*arg != '-') && (*arg != '/'))
        {
            Logger::Log(LOG_WARN, "Invalid command line argument `%s` at position %d", arg, pos);
            pos += 1;
            continue;
        }
        arg += (*(arg + 1) == '-') ? 2 : 1;

        HANDLE_ARG_VALUE("name",           { setServerName(value);         });
        HANDLE_ARG_VALUE("script-file",    { setScriptName(value);         });
        HANDLE_ARG_VALUE("terrain",        { setTerrain(value);            });
        HANDLE_ARG_VALUE("password",       { setPublicPass(value);         });
        HANDLE_ARG_VALUE("ip",             { setIPAddr(value);             });
        HANDLE_ARG_VALUE("resource-dir",   { setResourceDir(value);        });
        HANDLE_ARG_VALUE("auth-file",      { setAuthFile(value);           });
        HANDLE_ARG_VALUE("motd-file",      { setMOTDFile(value);           });
        HANDLE_ARG_VALUE("rules-file",     { setRulesFile(value);          });
        HANDLE_ARG_VALUE("owner",          { setOwner(value);              });
        HANDLE_ARG_VALUE("website",        { setWebsite(value);            });
        HANDLE_ARG_VALUE("irc",            { setIRC(value);                });
        HANDLE_ARG_VALUE("voip",           { setVoIP(value);               });
        HANDLE_ARG_VALUE("config-file",    { config_file = value;          });
        HANDLE_ARG_VALUE("c",              { config_file = value;          });

        HANDLE_ARG_VALUE("max-clients",    { setMaxClients(atoi(value));   });
        HANDLE_ARG_VALUE("webserver-port", { setWebserverPort(atoi(value));});
        HANDLE_ARG_VALUE("vehicle-limit",  { setMaxVehicles(atoi(value));  });
        HANDLE_ARG_VALUE("port",           { setListenPort(atoi(value));   });

        HANDLE_ARG_FLAG ("print-stats",    { setPrintStats(true);          });
        HANDLE_ARG_FLAG ("use-webserver",  { setWebserverEnabled(true);    });
        HANDLE_ARG_FLAG ("foreground",     { setForeground(true);          });
        HANDLE_ARG_FLAG ("fg",             { setForeground(true);          });
        HANDLE_ARG_FLAG ("inet",           { setServerMode(SERVER_INET);   });
        HANDLE_ARG_FLAG ("lan",            { setServerMode(SERVER_LAN);    });
        HANDLE_ARG_FLAG ("version",        { s_show_version = true;        });
        HANDLE_ARG_FLAG ("help",           { s_show_help = true;           });
        HANDLE_ARG_FLAG ("h",              { s_show_help = true;           });
        HANDLE_ARG_FLAG ("?",              { s_show_help = true;           });

        // Logging
        HANDLE_ARG_VALUE("log-file",      { Logger::SetOutputFile(value);                                });
        HANDLE_ARG_VALUE("verbosity",     { Logger::SetLogLevel(LOGTYPE_DISPLAY, (LogLevel)atoi(value)); });
        HANDLE_ARG_VALUE("log-verbosity", { Logger::SetLogLevel(LOGTYPE_FILE,    (LogLevel)atoi(value)); });

        Logger::Log(LOG_WARN, "Unrecognized argument `%s` at position %d", arg, pos);
        pos += 1;
    }

    if (config_file != nullptr)
    {
        loadConfigFile(config_file);
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
const std::string& GetServerlistHost()  { return s_serverlist_host;    }
bool               GetShowVersion()     { return s_show_version;       }
bool               GetShowHelp()        { return s_show_help;          }

unsigned int       GetHeartbeatRetryCount()   { return s_heartbeat_retry_count;  }
unsigned int       GetHeartbeatRetrySeconds() { return s_heatbeat_retry_seconds; }


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
        Logger::Log(LOG_ERROR, "could not generate server SHA1 password hash!");
        s_public_password = "";
        return false;
    }
    Logger::Log(LOG_DEBUG,"sha1(%s) = %s", pub_pass.c_str(), 
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
    Logger::Log(LOG_INFO, "loading config file %s ...", filename.c_str());
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

        if(config.exists("verbosity"))     Logger::SetLogLevel(LOGTYPE_DISPLAY,   (LogLevel)config.getIntValue("verbosity"));
        if(config.exists("logverbosity"))  Logger::SetLogLevel(LOGTYPE_FILE,      (LogLevel)config.getIntValue("logverbosity"));
        if(config.exists("resdir"))        setResourceDir(config.getStringValue   ("resdir"));
        if(config.exists("logfilename"))   Logger::SetOutputFile(config.getStringValue("logfilename"));
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
        Logger::Log(LOG_ERROR, "could not load config file %s : %s", filename.c_str(), config.getError());
    }
}

} //namespace Config

