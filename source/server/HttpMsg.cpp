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
}

HttpMsg::HttpMsg( const std::string& message )
{
    assign( message );
}

HttpMsg::~HttpMsg()
{
}


HttpMsg& HttpMsg::operator=( const std::string& message )
{
    assign( message );
    return *this;
}

HttpMsg& HttpMsg::operator=( const char* message )
{
    assign( message );
    return *this;
}

bool HttpMsg::operator==( const std::string& message )
{
    return getBody() == message;
}

const std::string& HttpMsg::getBody()
{
    return headermap["body"];
}

const std::vector<std::string> HttpMsg::getBodyLines()
{
    std::vector<std::string> lines;
    strict_tokenize( headermap["body"], lines, "\n" );
    return lines;
}

bool HttpMsg::isChunked()
{
    return "chunked" == headermap["Transfer-Encoding"];
}

void HttpMsg::assign( const std::string& message )
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
