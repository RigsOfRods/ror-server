#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>
#include <vector>
#include <boost/regex.hpp>


// server modes
enum ServerType {
	SERVER_LAN = 0,
	SERVER_INET,
	SERVER_AUTO
};

class Config
{
public:
	virtual ~Config();

	//! runs a check that all the required fields are present
	static bool checkConfig();
	static bool fromArgs( int argc, char* argv[] );

	//! checks if a password has been set for server access
	static bool isPublic();
	//! checks if the admin password is set, and the admin console is enabled.
	static bool hasAdmin();

	//! getter function
	//!@{
	static unsigned int       getMaxClients();
	static const std::string& getServerName();
	static const std::string& getTerrainName();
	static const std::string& getPublicPassword();
	static const std::string& getAdminPassword();
	static const std::string& getIPAddr();
	static unsigned int       getListenPort();
	static ServerType         getServerMode();
	//!@}

	//! setter functions
	//!@{
	static bool setMaxClients( unsigned int num );
	static bool setServerName( const std::string& name );
	static bool setTerrain(    const std::string& name );
	static bool setPublicPass( const std::string& name );
	static bool setAdminPass(  const std::string& name );
	static bool setIPAddr(     const std::string& name );
	static bool setListenPort( unsigned int port );
	static bool setServerMode( ServerType mode );
	//!@}

	static bool bannedUsername( const std::string& name );
	static bool bannedTruck(    const std::string& truck_file );
	static bool bannedIP(       const std::string& ip_addr );

	static void loadBannedUsernames(  const std::string& filename );
	static void loadBannedTrucks( const std::string& filename );
	static void loadBannedIPaddr( const std::string& filename );

private:
	Config();
	static Config instance;
    static bool bannedRegex( const std::string& checkVal,
            const std::vector<boost::regex>& regexs);

    static void loadRegexFromFile(  const std::string& filename,
            std::vector<boost::regex>& regexs);

	unsigned int max_clients;
	std::string server_name;
	std::string terrain_name;
	std::string public_password;
	std::string admin_password;
	std::string ip_addr;
	unsigned int listen_port;
	ServerType server_mode;

	std::vector<boost::regex> bannedNames;
    std::vector<boost::regex> bannedTrucks;
    std::vector<boost::regex> bannedIPs;



};

#endif /*CONFIG_H_*/
