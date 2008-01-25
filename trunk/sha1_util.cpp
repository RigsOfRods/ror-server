#include "sha1_util.h"

#include "sha1.h"
#include <iostream>
#include <iomanip>
#include <sstream>

bool toHex(char *result, unsigned char *data, unsigned int len)
{
	char tmp[20];
	unsigned int i=0;
	while(i<len)
	{
		memset(tmp, 0, 20);
		unsigned char d = (unsigned char)*data;
		sprintf(tmp, "%x", d);
		strcat(result, tmp);
		data++;
		i++;
	}
	return true;
}

bool SHA1FromString(char *result, char *source)
{
	// init variables
	char sha1result_bin[20];
	memset(sha1result_bin, 0, 20);
	char sha1result_hex[41];
	memset(sha1result_hex, 0, 41);

	// calculate hash of the filenames
	sha1( (unsigned char *)source, (int)strlen(source), (unsigned char *)sha1result_bin);
	bool res = toHex(sha1result_hex, (unsigned char*)sha1result_bin, 20);
	memcpy(result, sha1result_hex, 40);
	result[40]=0;
	return res;
}

bool SHA1FromString(std::string &result, std::string &sourceStr)
{

	// init variables
	char sha1result_bin[20];
	memset(sha1result_bin, 0, 20);
	char sha1result_hex[41];
	memset(sha1result_hex, 0, 41);

	// calculate hash of the filenames
	char *source = const_cast<char *>(sourceStr.c_str());
	sha1( (unsigned char *)source, (int)strlen(source), (unsigned char *)sha1result_bin);
	bool res = toHex(sha1result_hex, (unsigned char*)sha1result_bin, 20);
	if(res)
	{
		sha1result_hex[40]=0;
		result = std::string(sha1result_hex);
	}
	return res;
}

int getFileHash(char *filename, char *hash)
{
	FILE *cfd=fopen(filename, "rb");
	// anti-hacker :|
	if (cfd)
	{
		// obtain file size:
		fseek (cfd , 0 , SEEK_END);
		long lSize = ftell (cfd);
		rewind (cfd);

		// allocate memory to contain the whole file:
		char *buffer = (char*) malloc (sizeof(char)*(lSize+1));
		if (buffer == NULL) return -3;
		memset(buffer, 0, lSize);
		// copy the file into the buffer:
		size_t result = fread (buffer,1,lSize,cfd);
		if (result != lSize) return -2;
		// terminate
		fclose (cfd);
		buffer[lSize]=0;
		
		char sha1result[250];
		memset(sha1result, 0, 250);

		if(lSize<300)
			return -4;

		if(!SHA1FromString(sha1result, buffer))
		{
			return -1;
		}

		free (buffer);
		memcpy(hash, sha1result, 40);
		hash[41]=0;
		return 0;
	}
	return 1;
}

bool sha1check()
{
	char testStr[255]="The quick brown fox jumps over the lazy dog";
	char result[255]="";
	char testvalue[255]="2fd4e1c67a2d28fced849ee1bb76e7391b93eb12";
	SHA1FromString(result, testStr);
	return !strcmp(result, testvalue);
}
