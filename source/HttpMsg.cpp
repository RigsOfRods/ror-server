#include "HttpMsg.h"
#include "utils.h"

#include <stdexcept>
#include <sstream>
#include <iostream>

HttpMsg::HttpMsg( std::string message )
{
	std::size_t locHolder;
	locHolder = message.find("\r\n\r\n");
	std::vector<std::string> header;
	std::vector<std::string> tmp;
	
	strict_tokenize( message.substr( 0, locHolder ), header, "\r\n" );
	
	headermap["httpcode"] = header[0];
	for( unsigned short index = 1; index < header.size(); index++ )
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
	strict_tokenize( message.substr( locHolder ), tmp, "\r\n" );
	if( isChunked() )
		headermap["body"] = tmp[1];
	else
		headermap["body"] = tmp[0];
	

	
	// debuging stuff
	std::cout << "headermap contents " << headermap.size() << ": \n"; 
	for( std::map<std::string,std::string>::iterator it = headermap.begin();
		it != headermap.end();
		it++)
	{
		std::cout << "headermap[\"" << (*it).first << "\"]: "
				<< (*it).second << std::endl;
		
	}
}

HttpMsg::~HttpMsg()
{}

const std::string& HttpMsg::getBody()
{
	return headermap["body"];
}

bool HttpMsg::isChunked()
{
	return "chunked" == headermap["Transfer-Encoding"];
}
