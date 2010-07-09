#include "HttpMsg.h"
#include "utils.h"
#include "logger.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

HttpMsg::HttpMsg()
{
	STACKLOG;
}

HttpMsg::HttpMsg( const std::string& message )
{
	STACKLOG;
	assign( message );
}

HttpMsg::~HttpMsg()
{
	STACKLOG;
}


HttpMsg& HttpMsg::operator=( const std::string& message )
{
	STACKLOG;
	assign( message );
	return *this;
}

HttpMsg& HttpMsg::operator=( const char* message )
{
	STACKLOG;
	assign( message );
	return *this;
}

bool HttpMsg::operator==( const std::string& message )
{
	return getBody() == message;
}

const std::string& HttpMsg::getBody()
{
	STACKLOG;
	return headermap["body"];
}

const std::vector<std::string> HttpMsg::getBodyLines()
{
	STACKLOG;
	std::vector<std::string> lines;
	strict_tokenize( headermap["body"], lines, "\n" );
	return lines;
}

bool HttpMsg::isChunked()
{
	STACKLOG;
	return "chunked" == headermap["Transfer-Encoding"];
}

void HttpMsg::assign( const std::string& message )
{
	STACKLOG;
	std::size_t locHolder;
	locHolder = message.find("\r\n\r\n");
	std::vector<std::string> header;
	std::vector<std::string> tmp;
	
	strict_tokenize( message.substr( 0, locHolder ), header, "\r\n" );
	
	headermap["httpcode"] = header[0];
	for( unsigned short index = 1;
	header.size() >= 0 && index < header.size();
	index++ )
	{
		tmp.clear();
		tokenize( header[index], tmp, ":" );
		if( tmp.size() != 2 )
		{
			continue;
		}
		headermap[ trim( tmp[0] ) ] = trim( tmp[1] );
	}
	
	tmp.clear();
	locHolder = message.find_first_not_of("\r\n", locHolder);
	if( std::string::npos == locHolder )
	{
		std::string error_msg("Message does not appear to contain a body: \n");
		error_msg +=message;
		throw std::runtime_error( error_msg );
	}
	
	strict_tokenize( message.substr( locHolder ), tmp, "\r\n" );
	if( isChunked() )
		headermap["body"] = tmp[1];
	else
		headermap["body"] = tmp[0];
	
#if 0
	// debuging stuff
	std::cout << "headermap contents " << headermap.size() << ": \n"; 
	for( std::map<std::string,std::string>::iterator it = headermap.begin();
		it != headermap.end();
		it++)
	{
		std::cout << "headermap[\"" << (*it).first << "\"]: "
				<< (*it).second << std::endl;
		
	}
#endif
}
