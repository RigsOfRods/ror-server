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

#pragma once

#include "UnicodeStrings.h"

// server modes
enum ServerType {
    SERVER_LAN = 0,
    SERVER_INET,
    SERVER_AUTO
};

namespace Config {

//! runs a check that all the required fields are present
    bool checkConfig();

    bool ProcessArgs(int argc, char *argv[]);

    void ShowHelp();

    void ShowVersion();

//! checks if a password has been set for server access
    bool isPublic();

//! getter function
//!@{
    unsigned int getMaxClients();

    const std::string &getServerName();

    const std::string &getTerrainName();

    const std::string &getPublicPassword();

    const std::string &getIPAddr();

    const std::string &getScriptName();

    unsigned int getListenPort();

    ServerType getServerMode();

    bool getPrintStats();

    bool getEnableScripting();

    bool getWebserverEnabled();

    unsigned int getWebserverPort();

    bool getForeground();

    const std::string &getResourceDir();

    const std::string &getAuthFile();

    const std::string &getMOTDFile();

    const std::string &getRulesFile();

    const std::string &getBlacklistFile();

    unsigned int getMaxVehicles();

    const std::string &getOwner();

    const std::string &getWebsite();

    const std::string &getIRC();

    const std::string &getVoIP();

    const std::string &GetServerlistHost();

    const char *GetServerlistHostC();

    const std::string &GetServerlistPath();

    unsigned int GetHeartbeatRetryCount();

    unsigned int GetHeartbeatRetrySeconds();

    unsigned int GetHeartbeatIntervalSec();

    bool GetShowHelp();

    bool GetShowVersion();
//!@}

//! setter functions
//!@{
    bool setMaxClients(unsigned int num);

    bool setServerName(const std::string &name);

    bool setScriptName(const std::string &name);

    bool setTerrain(const std::string &name);

    bool setPublicPass(const std::string &name);

    bool setIPAddr(const std::string &name);

    bool setListenPort(unsigned int port);

    bool setServerMode(ServerType mode);

    void setPrintStats(bool value);

    void setWebserverEnabled(bool value);

    void setWebserverPort(unsigned int port);

    void setHeartbeatIntervalSec(unsigned sec);

    void setForeground(bool value);

    void LoadConfigFile(const std::string &filename);

    void setResourceDir(std::string dir);

    void setAuthFile(const std::string &file);

    void setMOTDFile(const std::string &file);

    void setRulesFile(const std::string &rulesFile);

    void setBlacklistFile(const std::string &blacklistFile);

    void setMaxVehicles(unsigned int num);

    void setOwner(const std::string &owner);

    void setWebsite(const std::string &website);

    void setIRC(const std::string &irc);

    void setVoIP(const std::string &voip);
//!@}

} // namespace Config

