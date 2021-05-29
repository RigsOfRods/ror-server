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

#include "spamfilter.h"

#include "config.h"
#include "logger.h"
#include "sequencer.h"

// --------------------------------
// Static functions

bool SpamFilter::IsActive()
{
    return (Config::getSpamFilterMsgIntervalSec() > 0) &&
           (Config::getSpamFilterMsgCount() > 0);
}

void SpamFilter::CheckConfig()
{
    if (SpamFilter::IsActive()) {
        Logger::Log(LOG_INFO, "spam filter: active, %d msg/%d sec -> %d sec gag",
            Config::getSpamFilterMsgCount(), Config::getSpamFilterMsgIntervalSec(),
            Config::getSpamFilterGagDurationSec());
    } else {
        Logger::Log(LOG_INFO, "spam filter: disabled");
    }
}

// --------------------------------
// Instance functions

SpamFilter::SpamFilter(Sequencer* seq, Client* c)
    : m_client(c)
    , m_sequencer(seq)
{}

bool SpamFilter::CheckForSpam(std::string const& msg) {
    const TimePoint now = std::chrono::system_clock::now();
    const std::chrono::seconds cache_expiry(Config::getSpamFilterMsgIntervalSec());

    // Count message occurences in cache
    auto itor = m_msg_cache.begin();
    int num_occurs = 0;
    while (itor != m_msg_cache.end()) {
        if (itor->first + cache_expiry < now) { // Purge expired messages
            itor = m_msg_cache.erase(itor);
        } else {
            if (msg == itor->second) { // Full, case-sensitive match -- proof of concept
                num_occurs++;
            }
            ++itor;
        }
    }

    // Add this message to cache
    m_msg_cache.push_back(CacheMsg(now, msg));

    // Update gag
    if (num_occurs > Config::getSpamFilterMsgCount()) {
        if (!m_gagged) {
            m_gag_expiry = now;
        }
        m_gagged = true;
        m_gag_expiry += std::chrono::seconds(Config::getSpamFilterGagDurationSec());
    } else if (m_gagged && m_gag_expiry < now) {
        m_gagged = false;
        m_sequencer->serverSay("Your gag has expired. Chat nicely!", m_client->GetUserId(), FROM_SERVER);
    }

    // Notify player
    if (m_gagged) {
        char msg[200];
        std::chrono::system_clock::duration time_left = m_gag_expiry - now;
        std::chrono::seconds seconds_left = std::chrono::duration_cast<std::chrono::seconds>(time_left);
        snprintf(msg, 200, "You are gagged. Time remaining: %d seconds.", (int)seconds_left.count());
        m_sequencer->serverSay(msg, m_client->GetUserId(), FROM_SERVER);
    }

    return m_gagged;
}
