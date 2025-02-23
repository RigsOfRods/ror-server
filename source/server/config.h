/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2025 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/
//// \file config.h

#pragma once

#include "UnicodeStrings.h"

/**
 * \brief Enum representing the server type
 */
enum ServerType {
    SERVER_LAN = 0,
    SERVER_INET,
    SERVER_AUTO
};

/**
 * \brief Enum representing the server power state
 */
enum ServerState {
    SERVER_STARTING = 0,
    SERVER_RUNNING,
    SERVER_STOPPING,
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

    bool getForeground();

    const std::string &getResourceDir();

    const std::string &getAuthFile();

    const std::string &getMOTDFile();

    const std::string &getRulesFile();

    const std::string &getBlacklistFile();

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

    const std::string &GetApiKeyKey();

    bool GetShowHelp();

    bool GetShowVersion();

    // Vehicle spawn limits
    unsigned int getMaxVehicles();
    int getSpawnIntervalSec();
    int getMaxSpawnRate();

    // Spam filter
    int getSpamFilterMsgIntervalSec();
    int getSpamFilterMsgCount();
    int getSpamFilterGagDurationSec();
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

    void setHeartbeatIntervalSec(unsigned sec);

    void setForeground(bool value);

    void LoadConfigFile(const std::string &filename);

    void setResourceDir(std::string dir);

    void setAuthFile(const std::string &file);

    void setMOTDFile(const std::string &file);

    void setRulesFile(const std::string &rulesFile);

    void setBlacklistFile(const std::string &blacklistFile);

    void setOwner(const std::string &owner);

    void setWebsite(const std::string &website);

    void setIRC(const std::string &irc);

    void setVoIP(const std::string &voip);

    // Vehicle spawn limits
    void setMaxVehicles(unsigned int num);
    void setSpawnIntervalSec(int sec);
    void setMaxSpawnRate(int num);

    // Spam filter
    void setSpamFilterMsgIntervalSec(int sec);
    void setSpamFilterMsgCount(int count);
    void setSpamFilterGagDurationSec(int sec);

    void setApiKeyKey(const std::string &key);
//!@}

} // namespace Config

