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

#include "sha1_util.h"

#include "sha1.h"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef __GNUC__
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif
#include <cstdio>

bool toHex(char *result, unsigned char *data, unsigned int len)
{
    char tmp[20];
    unsigned int i=0;
    while(i<len)
    {
        memset(tmp, 0, 20);
        unsigned char d = (unsigned char)*data;
        sprintf(tmp, "%02X", d);
        strcat(result, tmp);
        data++;
        i++;
    }
    return true;
}

bool SHA1FromString(char *result, const char *source)
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

bool SHA1FromBuffer(char *result, const char *source, int len)
{
    // init variables
    char sha1result_bin[20];
    memset(sha1result_bin, 0, 20);
    char sha1result_hex[2048];
    memset(sha1result_hex, 0, 2048);

    // calculate hash of the filenames
    sha1( (unsigned char *)source, len, (unsigned char *)sha1result_bin);
    bool res = toHex(sha1result_hex, (unsigned char*)sha1result_bin, 20);
    memcpy(result, sha1result_hex, 40);
    result[40]=0;
    return res;
}
bool SHA1FromString(std::string &result, const std::string &sourceStr)
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
        unsigned long lSize = ftell (cfd);
        if( lSize <= 0 ) { fclose(cfd); return 1; }
        rewind (cfd);

        // allocate memory to contain the whole file:
        char *buffer = (char*) malloc (sizeof(char)*(lSize+1));
        if (buffer == NULL) { fclose(cfd); return -3; }
        memset(buffer, 0, lSize);
        // copy the file into the buffer:
        size_t result = fread (buffer,1,lSize,cfd);
        if (result != lSize) { free(buffer); fclose(cfd); return -2; }
        // terminate
        fclose (cfd);
        buffer[lSize]=0;
        
        char sha1result[250];
        memset(sha1result, 0, 250);

        if(lSize<300)
            { free(buffer); return -4; }

        if(!SHA1FromString(sha1result, buffer))
        {
            free(buffer);
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
    char testvalue[255]="2FD4E1C67A2D28FCED849EE1BB76E7391B93EB12";
    SHA1FromString(result, testStr);
    return !strcmp(result, testvalue);
}
