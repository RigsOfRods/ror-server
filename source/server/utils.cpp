/*
 * This file is part of "Rigs of Rods Server" (Relay mode)
 *
 * Copyright 2007   Pierre-Michel Ricordel
 * Copyright 2014+  Rigs of Rods Community
 *
 * "Rigs of Rods Server" is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * "Rigs of Rods Server" is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with "Rigs of Rods Server". If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include "logger.h"
#include <string>
#include <vector>

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <iostream>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else

#include <sys/time.h>
#include <unistd.h>

#endif

namespace Utils
{
    int generateRandomPortNumber()
    {
        unsigned int tick_count = 0;

        // we need to be that precise here as it may happen that we start several servers at once, and thus the seed must be different
#ifdef _WIN32
        LARGE_INTEGER tick;
        QueryPerformanceCounter(&tick);
        tick_count = (unsigned int)tick.QuadPart;
#else   // _WIN32
        struct timeval now;
        gettimeofday(&now, NULL);
        tick_count = (now.tv_sec * 1000) + (now.tv_usec / 1000);
#endif  // _WIN32
        // init the random number generator
        srand(tick_count);
        return 12000 + (rand() % 1000);
    }

    int ReadLinesFromFile(std::string filename, std::vector<std::string> &lines)
    {
        FILE *f = fopen(filename.c_str(), "r");

        if (!f)
            return -1;

        int linecounter = 0;
        while (!feof(f))
        {
            char line[2048] = "";
            memset(line, 0, 2048);
            fgets(line, 2048, f);
            linecounter++;

            if (strnlen(line, 2048) <= 2)
                continue;

            // strip line (newline char)
            char *ptr = line;
            while (*ptr)
            {
                if (*ptr == '\n')
                {
                    *ptr = 0;
                    break;
                }
                ptr++;
            }
            lines.push_back(std::string(line));
        }
        fclose(f);
        return 0;
    }

    void SleepSeconds(unsigned int seconds)
    {
#ifndef _WIN32
        sleep(seconds);
#else
        Sleep(seconds * 1000);
#endif
    }
} // namespace Utils

using namespace std;

void tokenize(const std::string &str,
              std::vector<std::string> &tokens,
              const std::string &delimiters)
{
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);

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
void strict_tokenize(const std::string &str,
                     std::vector<std::string> &tokens,
                     const std::string &delimiter)
{
    size_t prev_loc = 0, new_loc = str.find(delimiter, prev_loc);

    while (new_loc < str.length() && prev_loc < str.length())
    {
        tokens.push_back(str.substr(prev_loc, new_loc - prev_loc));
        prev_loc = (new_loc > str.length() ? new_loc : new_loc + delimiter.length());
        new_loc  = str.find(delimiter, prev_loc);
    }

    tokens.push_back(str.substr(prev_loc, new_loc - prev_loc));
}

std::string trim(const std::string &str)
{
    if (!str.size())
        return str;

    return str.substr(str.find_first_not_of(" \t"), str.find_last_not_of(" \t") + 1);
}

std::string hexdump(void *pAddressIn, long lSize)
{
    char szBuf[100];
    long lIndent = 1;
    long lOutLen, lIndex, lIndex2, lOutLen2;
    long lRelPos;

    struct
    {
        char          *pData;
        unsigned long lSize;
    }             buf;
    unsigned char *pTmp, ucTmp;
    unsigned char *pAddress = (unsigned char*)pAddressIn;

    buf.pData = (char*)pAddress;
    buf.lSize = lSize;

    std::string result = "";

    while (buf.lSize > 0)
    {
        pTmp    = (unsigned char*)buf.pData;
        lOutLen = (int)buf.lSize;
        if (lOutLen > 16)
            lOutLen = 16;

        // create a 64-character formatted output line:
        sprintf(szBuf, " >                            "
                "                      "
                "    %08lX", (long unsigned int)(pTmp - pAddress));
        lOutLen2 = lOutLen;

        for (lIndex = 1 + lIndent, lIndex2 = 53 - 15 + lIndent, lRelPos = 0;
             lOutLen2;
             lOutLen2--, lIndex += 2, lIndex2++
        )
        {
            ucTmp = *pTmp++;

            sprintf(szBuf + lIndex, "%02X ", (unsigned short)ucTmp);
            if (!isprint(ucTmp))
                ucTmp = '.';                                                                                                                                                                                                                      // nonprintable char
            szBuf[lIndex2] = ucTmp;

            if (!(++lRelPos & 3))        // extra blank after 4 bytes
            {
                lIndex++;
                szBuf[lIndex + 2] = ' ';
            }
        }

        if (!(lRelPos & 3))
            lIndex--;

        szBuf[lIndex]     = '<';
        szBuf[lIndex + 1] = ' ';

        result += std::string(szBuf) + "\n";

        buf.pData += lOutLen;
        buf.lSize -= lOutLen;
    }
    return result;
}

// Calculates the length of an integer
int intlen(int num)
{
    int length = 0;

    if (num < 0)
    {
        num    = num * (-1);
        length = 1;   // set on 1 because of the minus sign
    }

    while (num > 0)
    {
        length++;
        num = num / 10;
    }
    return length;
}
