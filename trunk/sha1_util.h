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
	char output[20];
	memset(output, 0, 20);

	char output_hex[255];
	memset(output_hex, 0, 255);

	sha1( (unsigned char *)source, (int)strlen(source), (unsigned char *)output);
	if(!toHex(output_hex, output))
		return false;
	strncpy(result, output_hex, 255);
	return true;
}
