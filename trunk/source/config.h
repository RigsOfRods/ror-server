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

/*TODO the static in this class are fugly, I need to clean it up.
 * Sorry for for making it so :(
 *  - Ape
 */
class Config
{
public:
    virtual ~Config();

    //! runs a check that all the required fields are present
    static bool checkConfig();
    //! @return true if all parameters are valid for an Internet configuration
    static bool checkINETConfig();
    //! @return true if all parameters are valid for an Local configuration
    static bool checkLANConfig();

    static bool fromArgs( int argc, char* argv[] );

    //! checks if a password has been set for server access
    static bool isPublic();
    //! checks if the admin password is set, and the admin console is enabled.
    static bool hasAdmin();

    //! getter function
    //!@{
    static unsigned int       MaxClients();
    static const std::string& ServerName();
    static const std::string& TerrainName();
    static const std::string& PublicPassword();
    static const std::string& AdminPassword();
    static const std::string& IPAddr();
    static unsigned int       ListenPort();
    static ServerType         ServerMode();
    //!@}

    //! setter functions
    //!@{
    static bool MaxClients( unsigned int num );
    static bool ServerName( const std::string& name );
    static bool Terrain(    const std::string& name );
    static bool PublicPass( const std::string& name );
    static bool AdminPass(  const std::string& name );
    static bool IPAddr(     const std::string& name );
    static bool ListenPort( unsigned int port );
    static bool ServerMode( ServerType mode );
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

    static bool validIPAddr();
    static bool validINETClients();
    static bool validServerName();

    static bool validPort();
    static bool validTerrn();
    static bool validNumClients();

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
