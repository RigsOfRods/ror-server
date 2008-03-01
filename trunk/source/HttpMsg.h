#ifndef HTTPMSG_H_
#define HTTPMSG_H_
#include <string>
#include <vector>
#include <map>

class HttpMsg
{
public:
	HttpMsg( std::string message );
	virtual ~HttpMsg();
	
	const std::string& getBody();
	bool isChunked();
private:
	std::map<std::string, std::string> headermap;
};

#endif /*HTTPMSG_H_*/
