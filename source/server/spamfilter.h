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

#pragma once

/// @file Simple spam-filter for chat
/// @author Petr Ohlidal, 2020

#include "prerequisites.h"

#include <chrono>
#include <string>
#include <vector>

/// One instance per `Client` (see 'sequencer.h')
class SpamFilter
{
public:
    typedef std::chrono::system_clock::time_point  TimePoint; // Shorter code
    typedef std::pair<TimePoint, std::string>  CacheMsg;

    static bool IsActive();
    static void CheckConfig();

    SpamFilter(Sequencer* seq, Client* c);

    bool CheckForSpam(std::string const& msg); //!< True = it's spam!

private:
    std::vector<CacheMsg>  m_msg_cache;
    TimePoint              m_gag_expiry;
    Client*                m_client;
    Sequencer*             m_sequencer;
    bool                   m_gagged = false;
};

