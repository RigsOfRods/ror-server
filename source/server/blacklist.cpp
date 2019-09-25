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

#include <fstream>
#include <json/json.h>

Blacklist::Blacklist(Sequencer* database)
    : m_database(database)
{
}

void Blacklist::SaveBlacklistToFile()
{
    std::ofstream f;
    f.open(Config::getBlacklistFile(), std::ios::out);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LogLevel::LOG_WARN,
            "Couldn't open the local blacklist file ('%s'). Bans were not saved.",
            Config::getBlacklistFile().c_str());
        return;
    }

    Json::Value j_bans(Json::arrayValue);
    std::vector<ban_t> bans = m_database->GetBanListCopy();

    for (ban_t& ban : bans)
    {
        Json::Value j_ban(Json::objectValue);
        j_ban["ip"] = ban.ip;
        j_ban["nickname"] = ban.nickname;
        j_ban["banned_by_nickname"] = ban.bannedby_nick;
        j_ban["message"] = ban.banmsg;
        j_bans.append(j_ban);
    }

    Json::Value j_doc(Json::objectValue);
    j_doc["bans"] = j_bans;

    Json::StyledStreamWriter j_writer;
    j_writer.write(f, j_doc);
}

bool Blacklist::LoadBlacklistFromFile()
{
    std::ifstream f;
    f.open(Config::getBlacklistFile(), std::ios::in);
    if (!f.is_open() || !f.good())
    {
        Logger::Log(LogLevel::LOG_WARN,
                    "Couldn't open the local blacklist file ('%s'). No bans were loaded.",
                    Config::getBlacklistFile().c_str());
        return false;
    }

    Json::Value j_doc;
    Json::Reader j_reader;
    j_reader.parse(f, j_doc);
    if (!j_reader.good())
    {
        Logger::Log(LogLevel::LOG_WARN,
                    "Couldn't parse blacklist file, messages:\n%s",
                    j_reader.getFormattedErrorMessages());
        return false;
    }

    for (Json::Value& j_ban: j_doc["bans"])
    {
        m_database->RecordBan(
            -1, // User ID
            j_ban["ip"].asString(),
            j_ban["nickname"].asString(),
            j_ban["banned_by_nickname"].asString(),
            j_ban["message"].asString());
    }

    return true;
}
