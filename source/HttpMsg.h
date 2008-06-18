#ifndef HTTPMSG_H_
#define HTTPMSG_H_
#include <string>
#include <map>
#include <vector>


class HttpMsg
{
public:
	HttpMsg();
	HttpMsg( const std::string& message );
	virtual ~HttpMsg();
	
	const std::string& getBody();
	const std::vector<std::string> getBodyLines();
	bool isChunked();

	HttpMsg& operator=( const std::string& message );
	HttpMsg& operator=( const char* message );
	bool operator==( const std::string& message );
	
private:
	std::map<std::string, std::string> headermap;
	void assign( const std::string& message );
};

#endif /*HTTPMSG_H_*/
