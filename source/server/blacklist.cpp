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
along with Rigs of Rods Server. If not, see <http://www.gnu.org/licenses/>.
*/

#include "blacklist.h"
#include "config.h"
#include "logger.h"
#include "sequencer.h"
#include "utils.h"

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/JSONException.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/ParseHandler.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>

using namespace Poco;
using namespace Poco::JSON;
using namespace Poco::Dynamic;

#include <fstream>

Blacklist::Blacklist(Sequencer *database) : m_database(database)
{
}

void Blacklist::SaveBlacklistToFile()
{
    std::ofstream f;
    f.open(Config::getBlacklistFile(), std::ios::out);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LogLevel::LOG_WARN, "Couldn't open the local blacklist file ('%s'). Bans were not saved.",
                    Config::getBlacklistFile().c_str());
        return;
    }

    JSON::Array j_bans;
    std::vector<ban_t> bans = m_database->GetBanListCopy();

    for (ban_t &ban : bans)
    {
        DynamicStruct j_ban;
        j_ban["bid"] = ban.bid;
        j_ban["ip"] = ban.ip;
        j_ban["nickname"] = ban.nickname;
        j_ban["banned_by_nickname"] = ban.bannedby_nick;
        j_ban["message"] = ban.banmsg;
        j_bans.add(j_ban);
    }

    DynamicStruct j_doc;
    j_doc["bans"] = j_bans;

    Poco::JSON::Stringifier::stringify(j_doc, f, 1);
}

bool Blacklist::LoadBlacklistFromFile()
{
    std::ifstream f;
    f.open(Config::getBlacklistFile(), std::ios::in);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LogLevel::LOG_WARN, "Couldn't open the local blacklist file ('%s'). No bans were loaded.",
                    Config::getBlacklistFile().c_str());
        return false;
    }

    if (Utils::IsEmptyFile(f))
    {
        f.close();
        Logger::Log(LogLevel::LOG_WARN, "Local blacklist file ('%s') is empty.", Config::getBlacklistFile().c_str());
        return false;
    }

    Parser parser;
    Var result;

    try
    {
        result = parser.parse(f);
    }
    catch (JSONException &jsonException)
    {
        Logger::Log(LogLevel::LOG_WARN, "Couldn't parse blacklist file, messages:\n%s",
                    jsonException.message().c_str());
        return false;
    }

    try
    {
        Poco::JSON::Object::Ptr root = result.extract<Poco::JSON::Object::Ptr>();
        JSON::Array::Ptr bans = root->getArray("bans");

        for (int i = 0; i < bans->size(); ++i)
        {
            auto j_ban = bans->getObject(i);
            m_database->RecordBan(j_ban->getValue<std::string>("ip"),
                                  j_ban->getValue<std::string>("nickname"),
                                  j_ban->getValue<std::string>("banned_by_nickname"),
                                  j_ban->getValue<std::string>("message"));
        }
    }
    catch (Exception e)
    {
        Logger::Log(LogLevel::LOG_WARN, "Couldn't parse blacklist file, messages:\n%s", e.message().c_str());
    }
    return true;
}
