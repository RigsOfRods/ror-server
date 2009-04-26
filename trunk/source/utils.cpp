
#include "utils.h"
#include "logger.h"
#include <string>
#include <vector>

void tokenize(const std::string& str,
				std::vector<std::string>& tokens,
				const std::string& delimiters)
{
	STACKLOG;
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
	while (std::string::npos != pos || std::string::npos != lastPos)
	{
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}

//! strict_tokenize treats the provided delimiter as the entire delimiter
//! with tokenize, the string "  " would result in an empty vector.
//! with strict_tokenize the same string would result in a vector with
//! a single empty string.
void strict_tokenize(const std::string& str,
				std::vector<std::string>& tokens,
				const std::string& delimiter)
{
	size_t prev_loc = 0, new_loc = str.find( delimiter, prev_loc );
	
	while( new_loc < str.length() && prev_loc < str.length() ) {
		tokens.push_back( str.substr( prev_loc, new_loc - prev_loc ) );
		prev_loc = ( new_loc > str.length() ? new_loc : new_loc + delimiter.length() );
		new_loc = str.find( delimiter, prev_loc ); 
	}
	
	tokens.push_back( str.substr( prev_loc, new_loc - prev_loc ) );
}

std::string trim(const std::string& str )
{
	STACKLOG;
	if(!str.size()) return str;
	return str.substr( str.find_first_not_of(" \t"), str.find_last_not_of(" \t")+1);
}
