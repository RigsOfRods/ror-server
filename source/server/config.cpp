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
#include "spamfilter.h"
#include "utils.h"

#include <cmath>
#include <cstring>

#ifdef __GNUC__

#include <unistd.h>
#include <stdlib.h>

#endif

#ifdef _WIN32
#   define RESOURCE_DIR ""
#else // _WIN32 ~ trailing slash important
#   define RESOURCE_DIR "/usr/share/rorserver/"
#endif // _WIN32

#define CONFIG_LINE_BUF_LEN 2000

// ============================== Variables ===================================

static std::string s_server_name;
static std::string s_terrain_name("any");
static std::string s_public_password;
static std::string s_ip_addr("0.0.0.0");
static std::string s_scriptname;
static std::string s_authfile("server.auth");
static std::string s_motdfile("server.motd");
static std::string s_rulesfile("server.rules");
static std::string s_blacklistfile("server.blacklist");
static std::string s_owner;
static std::string s_website;
static std::string s_irc;
static std::string s_voip;
static std::string s_serverlist_host("api.rigsofrods.org");
static std::string s_serverlist_path("");
static std::string s_resourcedir(RESOURCE_DIR);

static unsigned int s_webserver_port(0);
static unsigned int s_listen_port(0);
static unsigned int s_max_clients(16);
static unsigned int s_heartbeat_retry_count(5);
static unsigned int s_heartbeat_retry_seconds(15);
static unsigned int s_heartbeat_interval_sec(60);

static bool s_print_stats(false);
static bool s_foreground(false);
static bool s_show_version(false);
static bool s_show_help(false);
static bool s_webserver_enabled(false);

// Vehicle spawn limits
static size_t s_max_vehicles(20);
static int    s_spawn_interval_sec(0);
static int    s_max_spawn_rate(0);

static ServerType s_server_mode(SERVER_AUTO);

static int s_spamfilter_msg_interval_sec(0); // 0 disables spamfilter
static int s_spamfilter_msg_count(0); // 0 disables spamfilter
static int s_spamfilter_gag_duration_sec(10);

// ============================== Functions ===================================

namespace Config {

    void ShowHelp() {
        printf(
                "Usage: rorserver [OPTIONS]\n"
                        "[OPTIONS] can be in Un*x `--help` or windows `/help` notation\n"
                        "\n"
                        " -config-file (-c) <INI file> Loads the configuration from a file\n"
                        " -name <name>                 Name of the server, no spaces, only\n"
                        "                              [a-z,0-9,A-Z]\n"
                        " -terrain <mapname>           Map name (defaults to 'any')\n"
                        " -max-clients|speed <clients> Maximum clients allowed\n"
                        " -lan|inet                    Private or public server (defaults to inet)\n"
                        "\n"
                        " -password <password>         Private server password\n"
                        " -ip <ip>                     Public IP address to register with.\n"
                        " -port <port>                 Port to use (defaults to random 12000-12500)\n"
                        " -verbosity {0-5}             Sets displayed log verbosity\n"
                        " -log-verbosity {0-5}         Sets file log verbositylog verbosity\n"
                        "                              levels available to verbosity and logverbosity:\n"
                        "                                  0 = stack\n"
                        "                                  1 = debug\n"
                        "                                  2 = verbosity\n"
                        "                                  3 = info\n"
                        "                                  4 = warn\n"
                        "                                  5 = error\n"
                        " -log-file <server.log>       Sets the filename of the log\n"
                        " -script-file <script.as>     Server script to execute\n"
                        " -print-stats                 Prints stats to the console\n"
                        " -version                     Prints the server version numbers\n"
                        " -fg                          Starts the server in the foreground (background by default)\n"
                        " -resource-dir <path>         Sets the path to the resource directory\n"
                        " -auth-file <server.auth>             Path to file with authorization info\n"
                        " -motd-file <server.motd>             Path to file with message of the day\n"
                        " -rules-file <server.rules>           Path to file with rules for this server\n"
                        " -blacklist-file <server.blacklist>   Path to file where bans are persisted\n"
                        " -vehicle-limit {0-...}       Sets the maximum number of vehicles that a user is allowed to have\n"
                        " -owner <name|organisation>   Sets the owner of this server (for the !owner command) (optional)\n"
                        " -website <URL>               Sets the website of this server (for the !website command) (optional)\n"
                        " -irc <URL>                   Sets the IRC url for this server (for the !irc command) (optional)\n"
                        " -voip <URL>                  Sets the voip url for this server (for the !voip command) (optional)\n"
                        " -help                        Show this list\n");
    }

    void ShowVersion() {
        printf("Rigs of Rods Server\n");
        printf(" * using Protocol %s\n", RORNET_VERSION);
        printf(" * built on %s, %s\n", __DATE__, __TIME__);
#ifdef __GNUC__
        printf(" * built with gcc %d.%d.%d\n", __GNUC_MINOR__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif //__GNUC__
    }

//! runs a check that all the required fields are present
    bool checkConfig() {
        switch (getServerMode()) {
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

        if (!getListenPort()) {
            Logger::Log(LOG_WARN, "No port supplied, randomly generating one");
            setListenPort(Utils::generateRandomPortNumber());
        }

        Logger::Log(LOG_INFO, "port:       %d", getListenPort());

        if (getTerrainName().empty()) {
            Logger::Log(LOG_ERROR, "terrain not specified");
            return 0;
        } else
            Logger::Log(LOG_INFO, "terrain:    %s", getTerrainName().c_str());

        if (getMaxClients() < 2 || getMaxClients() > 64) {
            Logger::Log(LOG_ERROR, "Max clients need to 2 or more, and 64 or less.");
            return 0;
        } else
            Logger::Log(LOG_INFO, "maxclients: %d", getMaxClients());

        if (getAuthFile().empty()) {
            Logger::Log(LOG_ERROR, "Authorizations file not specified. Using default (admins.txt)");
            setAuthFile("server.auth");
        }

        if (getMOTDFile().empty()) {
            Logger::Log(LOG_ERROR, "MOTD file not specified. Using default (motd.txt).");
            setMOTDFile("server.motd");
        }

        if (getMaxVehicles() < 1) {
            Logger::Log(LOG_ERROR, "The vehicle-limit cannot be less than 1!");
            return 0;
        }

        SpamFilter::CheckConfig();

        Logger::Log(LOG_INFO, "server is%s password protected",
                    getPublicPassword().empty() ? " NOT" : "");

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
                "Command line error: argument `%s` "\
                "at position %d has no value",      \
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

    bool ProcessArgs(int argc, char *argv[]) {
#ifndef NOCMDLINE
        int pos = 1;
        const char *config_file = nullptr;
        while (pos < argc) {
            // Cut off the leading `-`, `--` or `/` (windows)
            char *arg = argv[pos];
            if ((*arg != '-') && (*arg != '/')) {
                Logger::Log(LOG_WARN, "Invalid command line argument `%s` at position %d", arg, pos);
                pos += 1;
                continue;
            }
            arg += (*(arg + 1) == '-') ? 2 : 1;

            HANDLE_ARG_VALUE("name", { setServerName(value); });
            HANDLE_ARG_VALUE("script-file", { setScriptName(value); });
            HANDLE_ARG_VALUE("terrain", { setTerrain(value); });
            HANDLE_ARG_VALUE("password", { setPublicPass(value); });
            HANDLE_ARG_VALUE("ip", { setIPAddr(value); });
            HANDLE_ARG_VALUE("resource-dir", { setResourceDir(value); });
            HANDLE_ARG_VALUE("auth-file", { setAuthFile(value); });
            HANDLE_ARG_VALUE("motd-file", { setMOTDFile(value); });
            HANDLE_ARG_VALUE("rules-file", { setRulesFile(value); });
            HANDLE_ARG_VALUE("blacklist-file", { setBlacklistFile(value); });
            HANDLE_ARG_VALUE("owner", { setOwner(value); });
            HANDLE_ARG_VALUE("website", { setWebsite(value); });
            HANDLE_ARG_VALUE("irc", { setIRC(value); });
            HANDLE_ARG_VALUE("voip", { setVoIP(value); });
            HANDLE_ARG_VALUE("config-file", { config_file = value; });
            HANDLE_ARG_VALUE("c", { config_file = value; });

            HANDLE_ARG_VALUE("max-clients", { setMaxClients(atoi(value)); });
            HANDLE_ARG_VALUE("vehicle-limit", { setMaxVehicles(atoi(value)); });
            HANDLE_ARG_VALUE("port", { setListenPort(atoi(value)); });
            HANDLE_ARG_VALUE("webserver-port", { setWebserverPort(atoi(value)); });

            HANDLE_ARG_FLAG ("print-stats", { setPrintStats(true); });
            HANDLE_ARG_FLAG ("use-webserver", { setWebserverEnabled(true); });
            HANDLE_ARG_FLAG ("foreground", { setForeground(true); });
            HANDLE_ARG_FLAG ("fg", { setForeground(true); });
            HANDLE_ARG_FLAG ("inet", { setServerMode(SERVER_INET); });
            HANDLE_ARG_FLAG ("lan", { setServerMode(SERVER_LAN); });
            HANDLE_ARG_FLAG ("version", { s_show_version = true; });
            HANDLE_ARG_FLAG ("help", { s_show_help = true; });
            HANDLE_ARG_FLAG ("h", { s_show_help = true; });
            HANDLE_ARG_FLAG ("?", { s_show_help = true; });

            // Logging
            HANDLE_ARG_VALUE("log-file", { Logger::SetOutputFile(value); });
            HANDLE_ARG_VALUE("verbosity", { Logger::SetLogLevel(LOGTYPE_DISPLAY, (LogLevel) atoi(value)); });
            HANDLE_ARG_VALUE("log-verbosity", { Logger::SetLogLevel(LOGTYPE_FILE, (LogLevel) atoi(value)); });

            Logger::Log(LOG_WARN, "Unrecognized argument `%s` at position %d", arg, pos);
            pos += 1;
        }

        if (config_file != nullptr) {
            LoadConfigFile(config_file);
        }
#endif //NOCMDLINE
        return true;
    }

    bool isPublic() { return !getPublicPassword().empty(); }

    unsigned int getMaxClients() { return s_max_clients; }

    const std::string &getServerName() { return s_server_name; }

    const std::string &getTerrainName() { return s_terrain_name; }

    const std::string &getPublicPassword() { return s_public_password; }

    const std::string &getIPAddr() { return s_ip_addr; }

    const std::string &getScriptName() { return s_scriptname; }

    bool getEnableScripting() { return (s_scriptname != ""); }

    unsigned int getListenPort() { return s_listen_port; }

    ServerType getServerMode() { return s_server_mode; }

    bool getWebserverEnabled() { return s_webserver_enabled; }

    unsigned int getWebserverPort() { return s_webserver_port; }

    bool getPrintStats() { return s_print_stats; }

    bool getForeground() { return s_foreground; }

    const std::string &getResourceDir() { return s_resourcedir; }

    const std::string &getAuthFile() { return s_authfile; }

    const std::string &getMOTDFile() { return s_motdfile; }

    const std::string &getBlacklistFile() { return s_blacklistfile; }

    const std::string &getRulesFile() { return s_rulesfile; }

    const std::string &getOwner() { return s_owner; }

    const std::string &getWebsite() { return s_website; }

    const std::string &getIRC() { return s_irc; }

    const std::string &getVoIP() { return s_voip; }

    const std::string &GetServerlistHost() { return s_serverlist_host; }

    const char *GetServerlistHostC() { return s_serverlist_host.c_str(); }

    const std::string &GetServerlistPath() { return s_serverlist_path; }

    bool GetShowVersion() { return s_show_version; }

    bool GetShowHelp() { return s_show_help; }

    unsigned int GetHeartbeatRetryCount() { return s_heartbeat_retry_count; }

    unsigned int GetHeartbeatRetrySeconds() { return s_heartbeat_retry_seconds; }

    unsigned int GetHeartbeatIntervalSec() { return s_heartbeat_interval_sec; }

    unsigned int getMaxVehicles() { return s_max_vehicles; }

    int getSpawnIntervalSec() { return s_spawn_interval_sec; }

    int getMaxSpawnRate() { return s_max_spawn_rate; }

    int getSpamFilterMsgIntervalSec() { return s_spamfilter_msg_interval_sec; }

    int getSpamFilterMsgCount() { return s_spamfilter_msg_count; }

    int getSpamFilterGagDurationSec() { return s_spamfilter_gag_duration_sec; }

    bool setScriptName(const std::string &name) {
        if (name.empty()) return false;
        s_scriptname = name;
        return true;
    }

    bool setMaxClients(unsigned int num) {
        if (num < 2 || num > 64) return false;
        s_max_clients = num;
        return true;
    }

    bool setServerName(const std::string &name) {
        if (name.empty()) return false;
        s_server_name = name;
        return true;
    }

    bool setTerrain(const std::string &tern) {
        if (tern.empty()) return false;
        s_terrain_name = tern;
        return true;
    }

    bool setPublicPass(const std::string &pub_pass) {
        if (pub_pass.length() > 0 && pub_pass.size() < 250 &&
            !SHA1FromString(s_public_password, pub_pass)) {
            Logger::Log(LOG_ERROR, "could not generate server SHA1 password hash!");
            s_public_password = "";
            return false;
        }
        Logger::Log(LOG_DEBUG, "sha1(%s) = %s", pub_pass.c_str(),
                    s_public_password.c_str());
        return true;
    }

    bool setIPAddr(const std::string &ip) {
        if (ip.empty()) return false;
        s_ip_addr = ip;
        return true;
    }

    bool setListenPort(unsigned int port) {
        s_listen_port = port;
        return true;
    }

    void setWebserverPort(unsigned int port) { s_webserver_port = port; }

    void setWebserverEnabled(bool webserver) { s_webserver_enabled = webserver; }

    void setPrintStats(bool value) { s_print_stats = value; }

    void setAuthFile(const std::string &file) { s_authfile = file; }

    void setMOTDFile(const std::string &file) { s_motdfile = file; }

    void setRulesFile(const std::string &file) { s_rulesfile = file; }

    void setBlacklistFile(const std::string &file) { s_blacklistfile = file; }

    void setMaxVehicles(unsigned int num) { s_max_vehicles = num; }

    void setSpawnIntervalSec(int sec) { s_spawn_interval_sec = sec; }

    void setMaxSpawnRate(int num) { s_max_spawn_rate = num; }

    void setOwner(const std::string &owner) { s_owner = owner; }

    void setForeground(bool value) { s_foreground = value; }

    void setWebsite(const std::string &website) { s_website = website; }

    void setIRC(const std::string &irc) { s_irc = irc; }

    void setVoIP(const std::string &voip) { s_voip = voip; }

    void setSpamFilterMsgIntervalSec(int sec) { s_spamfilter_msg_interval_sec = sec; }

    void setSpamFilterMsgCount(int count) { s_spamfilter_msg_count = count; }

    void setSpamFilterGagDurationSec(int sec) { s_spamfilter_gag_duration_sec = sec; }

    void setHeartbeatIntervalSec(unsigned sec) {
        s_heartbeat_interval_sec = sec;
        Logger::Log(LOG_VERBOSE, "Hearbeat interval is %d seconds", sec);
    }

    bool setServerMode(ServerType mode) {
        s_server_mode = mode;
        return true;
    }

    void setResourceDir(std::string dir) {
        if (dir.length() > 0 && dir.substr(dir.length() - 1) != "/")
            dir += "/";
        s_resourcedir = dir;
    }

    inline void SetConfServerMode(std::string const &val) {
        if (val.compare("inet") == 0)
            setServerMode(SERVER_INET);
        else
            setServerMode(SERVER_LAN);
    }

// Cloned from RudeConfig (http://rudeserver.com/config/) we used before. #compatibility
    inline bool RudeStrToBool(const char *val) {
        switch (val[0]) {
            case 't':
            case 'T':
            case 'y':
            case 'Y':
            case '1':
                return true;
            case 'o':
            case 'O':
                return (val[1] == 'n' || val[1] == 'N');
            default:
                return false;
        }
    }

#define VAL_STR(_STR_) (Str::SanitizeUtf8(_STR_))
#define VAL_INT(_STR_) (atoi(value)) // Cloned from RudeConfig we used before. #compatibility
#define VAL_BOOL(_STR_)(RudeStrToBool(value))

    void ProcessConfigEntry(const char *key, const char *value) {
        if (strcmp(key, "baseconfig") == 0) { LoadConfigFile(VAL_STR (value)); }
        else if (strcmp(key, "slots") == 0) { setMaxClients(VAL_INT (value)); }
        else if (strcmp(key, "name") == 0) { setServerName(VAL_STR (value)); }
        else if (strcmp(key, "scriptname") == 0) { setScriptName(VAL_STR (value)); }
        else if (strcmp(key, "terrain") == 0) { setTerrain(VAL_STR (value)); }
        else if (strcmp(key, "password") == 0) { setPublicPass(VAL_STR (value)); }
        else if (strcmp(key, "ip") == 0) { setIPAddr(VAL_STR (value)); }
        else if (strcmp(key, "port") == 0) { setListenPort(VAL_INT (value)); }
        else if (strcmp(key, "mode") == 0) { SetConfServerMode(VAL_STR (value)); }
        else if (strcmp(key, "printstats") == 0) { setPrintStats(VAL_BOOL(value)); }
        else if (strcmp(key, "webserver") == 0) { setWebserverEnabled(VAL_BOOL(value)); }
        else if (strcmp(key, "webserverport") == 0) { setWebserverPort(VAL_INT (value)); }
        else if (strcmp(key, "foreground") == 0) { setForeground(VAL_BOOL(value)); }
        else if (strcmp(key, "resdir") == 0) { setResourceDir(VAL_STR (value)); }
        else if (strcmp(key, "logfilename") == 0) { Logger::SetOutputFile(VAL_STR (value)); }
        else if (strcmp(key, "authfile") == 0) { setAuthFile(VAL_STR (value)); }
        else if (strcmp(key, "motdfile") == 0) { setMOTDFile(VAL_STR (value)); }
        else if (strcmp(key, "rulesfile") == 0) { setRulesFile(VAL_STR (value)); }
        else if (strcmp(key, "blacklistfile") == 0) { setBlacklistFile(VAL_STR(value)); }
        else if (strcmp(key, "owner") == 0) { setOwner(VAL_STR (value)); }
        else if (strcmp(key, "website") == 0) { setWebsite(VAL_STR (value)); }
        else if (strcmp(key, "irc") == 0) { setIRC(VAL_STR (value)); }
        else if (strcmp(key, "voip") == 0) { setVoIP(VAL_STR (value)); }
        else if (strcmp(key, "serverlist-host") == 0) { s_serverlist_host = VAL_STR (value); }
        else if (strcmp(key, "serverlist-path") == 0) { s_serverlist_path = VAL_STR (value); }
        else if (strcmp(key, "verbosity") == 0) { Logger::SetLogLevel(LOGTYPE_DISPLAY, (LogLevel) VAL_INT(value)); }
        else if (strcmp(key, "logverbosity") == 0) { Logger::SetLogLevel(LOGTYPE_FILE, (LogLevel) VAL_INT(value)); }
        else if (strcmp(key, "heartbeat-interval") == 0) { setHeartbeatIntervalSec(VAL_INT(value)); }

        // Vehicle spawn limits
        else if (strcmp(key, "vehiclelimit") == 0) { setMaxVehicles(VAL_INT (value)); }
        else if (strcmp(key, "vehicle-spawn-interval") == 0) { setSpawnIntervalSec(VAL_INT (value)); }
        else if (strcmp(key, "vehicle-max-spawn-rate") == 0) { setMaxSpawnRate(VAL_INT (value)); }

        // Spam filter
        else if (strcmp(key, "spamfilter-msg-interval") == 0) { setSpamFilterMsgIntervalSec(VAL_INT(value)); }
        else if (strcmp(key, "spamfilter-msg-count")    == 0) { setSpamFilterMsgCount(VAL_INT(value)); }
        else if (strcmp(key, "spamfilter-gag-duration") == 0) { setSpamFilterGagDurationSec(VAL_INT(value)); }

        else {
            Logger::Log(LOG_WARN, "Unknown key '%s' (value: '%s') in config file.", key, value);
        }
    }

    void LoadConfigFile(const std::string &filename) {
        Logger::Log(LOG_INFO, "loading config file %s ...", filename.c_str());

        FILE *f = fopen(filename.c_str(), "r");
        if (f == nullptr) {
            Logger::Log(LOG_ERROR, "Failed to open config file %s ...", filename.c_str());
            return;
        }

        size_t line_num = 0;
        while (!feof(f) && !ferror(f)) {
            ++line_num;

            char line_buf[CONFIG_LINE_BUF_LEN];
            if (fgets(line_buf, CONFIG_LINE_BUF_LEN, f) == nullptr) {
                break;
            }

            char *key_start = line_buf;
            char *val_end = line_buf + strlen(line_buf);
            Str::TrimAscii(key_start, val_end); // In-out

            if (key_start == val_end)
                continue; // Skip empty lines

            if (*key_start == '#')
                continue; // Skip comment lines

            char *key_end = strrchr(key_start, '=');
            if (key_end == nullptr) {
                Logger::Log(LOG_ERROR, "Invalid line %u; missing '=' separator (config file %s)", line_num,
                            filename.c_str());
                continue; // Skip invalid line
            }

            char *val_start = key_end + 1;
            Str::TrimAscii(key_start, key_end); // In-out
            Str::TrimAscii(val_start, val_end);
            *key_end = '\0';
            *val_end = '\0';
            ProcessConfigEntry(key_start, val_start);
        }

        if (!feof(f)) {
            Logger::Log(LOG_ERROR, "Error reading line %u from config file %s", line_num, filename.c_str());
        }
        fclose(f);
    }

} //namespace Config

