#pragma once

#include <string>

// server modes
enum ServerType
{
	SERVER_LAN = 0,
	SERVER_INET,
	SERVER_AUTO
};

namespace Config
{

//! runs a check that all the required fields are present
bool checkConfig();
bool fromArgs( int argc, char* argv[] );

//! checks if a password has been set for server access
bool isPublic();

//! getter function
//!@{
unsigned int       getMaxClients();
const std::string& getServerName();
const std::string& getTerrainName();
const std::string& getPublicPassword();
const std::string& getIPAddr();
const std::string& getScriptName();
unsigned int       getListenPort();
ServerType         getServerMode();
bool               getPrintStats();
bool               getEnableScripting();
bool               getWebserverEnabled();
unsigned int       getWebserverPort();
bool               getForeground();
const std::string& getResourceDir();
const std::string& getAuthFile();
const std::string& getMOTDFile();
const std::string& getRulesFile();
unsigned int       getMaxVehicles();
const std::string& getOwner();
const std::string& getWebsite();
const std::string& getIRC();
const std::string& getVoIP();
//!@}

//! setter functions
//!@{
bool setMaxClients(unsigned int num);
bool setServerName( const std::string& name );
bool setScriptName( const std::string& name );
bool setTerrain( const std::string& name );
bool setPublicPass( const std::string& name );
bool setIPAddr( const std::string& name );
bool setListenPort( unsigned int port );
bool setServerMode( ServerType mode);
void setPrintStats(bool value);
void setWebserverEnabled(bool value);
void setWebserverPort( unsigned int port );
void setForeground(bool value);
void loadConfigFile(const std::string& filename);
void setResourceDir(std::string dir);
void setAuthFile(const std::string& file);
void setMOTDFile(const std::string& file);
void setRulesFile(const std::string& rulesFile);
void setMaxVehicles(unsigned int num);
void setOwner(const std::string& owner);
void setWebsite(const std::string& website);
void setIRC(const std::string& irc);
void setVoIP(const std::string& voip);
//!@}

} // namespace Config

