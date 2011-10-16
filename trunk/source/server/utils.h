#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>

void tokenize(const std::string& str,
				std::vector<std::string>& tokens,
				const std::string& delimiters = " ");

//! strict_tokenize treats the provided delimiter as the entire delimiter
//! with tokenize, the string "  " would result in an empty vector.
//! with strict_tokenize the same string would result in a vector with
//! a single empty string.
void strict_tokenize(const std::string& str,
				std::vector<std::string>& tokens,
				const std::string& delimiter = " ");

template <class T>
T from_string(const std::string& s, 
                 std::ios_base& (*f)(std::ios_base&))
{
	
	std::istringstream iss(s);
	T t;
	if( !(iss >> f >> t).fail() )
	{
		
		throw std::runtime_error(s + " is not a numerical value");
	}
	return t;
}

std::string trim(const std::string& str );

// from http://stahlforce.com/dev/index.php?tool=csc01
std::string hexdump(void *pAddressIn, long  lSize);

int intlen(int num);

// Prototype for conversion functions
std::string narrow(const std::wstring& wcs);
std::wstring widen(const std::string& mbs);

// UTF8 conversions
template <int limit> 
std::wstring UTF8toWString(const char *str)
{
	wchar_t tmp[limit];
	mbstowcs(tmp, str, limit);
	return std::wstring(tmp);
}

template <int limit> 
void wStringtoUTF8String(char *str, std::wstring w)
{
	wcstombs(str, w.c_str(), limit);
}

template <int limit> 
std::string UTF8toString(const char *str)
{
	return narrow(UTF8toWString<limit>(str));
}

#endif /*UTILS_HPP_*/
