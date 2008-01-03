#include "sha1.h"

#include <iostream>
#include <sstream>
#include <iomanip>

bool toHex(char *result, char *data)
{
	std::ostringstream hexStream;
	hexStream << std::setw(2) << std::setfill('0') << std::hex;
	for(unsigned int i=0;i<strnlen(data, 2048);i++)
	{
		hexStream << static_cast<unsigned int>(static_cast<unsigned char>(data[i]));
	}
	strcpy(result, hexStream.str().c_str());
	return true;
}

bool SHA1FromString(char *result, char *source)
{
	char src[2048]="";
	char tmp[60]="", tmp1[60]="";

	strncpy(src, source, 2048);
	sha1( (unsigned char *)src, (int)strnlen(src, 2048), (unsigned char *)tmp);
	result[60]=0;
	if(!toHex(tmp1, tmp))
		return false;
	strncpy(result, tmp1, 60);
	return true;
}
