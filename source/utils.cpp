
#include "utils.h"
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>

void tokenize(const std::string& str,
				std::vector<std::string>& tokens,
				const std::string& delimiters)
{
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
	// Skip delimiter at beginning.
	std::string::size_type lastPos = str.find(delimiter, 0);
	if( 0 == lastPos) lastPos = delimiter.length();
	else lastPos = 0;
	// Find first "non-delimiter".
	std::string::size_type pos     = str.find(delimiter, lastPos);
	while (std::string::npos != pos || std::string::npos != lastPos)
	{
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find(delimiter, pos);
		if( lastPos == pos && lastPos != std::string::npos )
		{
			lastPos += delimiter.length();
			if( lastPos >= str.length() )
				lastPos = std::string::npos;
		}
		
		// Find next "non-delimiter"
		pos = str.find(delimiter, lastPos);
	}
}

std::string trim(const std::string& str )
{
	return str.substr( str.find_first_not_of(" \t"), str.find_last_not_of(" \t")+1);
}
