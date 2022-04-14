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

#pragma once

#include "UnicodeStrings.h"

#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <Poco/Dynamic/Struct.h>

namespace Utils {

    int generateRandomPortNumber();

    int ReadLinesFromFile(::std::string filename, std::vector<std::string> &lines);

    void SleepSeconds(unsigned int seconds);

    bool IsEmptyFile(std::ifstream& f);

    // For checking STREAM_REGISTER messages
    bool isValidVehicleFileName(std::string name);


} // namespace Utils

void tokenize(const std::string &str,
              std::vector<std::string> &tokens,
              const std::string &delimiters = " ");

//! strict_tokenize treats the provided delimiter as the entire delimiter
//! with tokenize, the string "  " would result in an empty vector.
//! with strict_tokenize the same string would result in a vector with
//! a single empty string.
void strict_tokenize(const std::string &str,
                     std::vector<std::string> &tokens,
                     const std::string &delimiter = " ");

template<class T>
T from_string(const std::string &s,
              std::ios_base &(*f)(std::ios_base &)) {

    std::istringstream iss(s);
    T t;
    if (!(iss >> f >> t).fail()) {

        throw std::runtime_error(s + " is not a numerical value");
    }
    return t;
}

std::string trim(const std::string &str);

// from http://stahlforce.com/dev/index.php?tool=csc01
std::string hexdump(void *pAddressIn, long lSize);

int intlen(int num);

std::string JsonToString(const Poco::DynamicStruct data);