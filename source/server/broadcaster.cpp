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

#include "broadcaster.h"

#include "logger.h"
#include "messaging.h"
#include "sequencer.h"

#include <map>
#include <algorithm>

Broadcaster::Broadcaster() :
        m_is_dropping_packets(false),
        m_packet_drop_counter(0),
        m_packet_good_counter(0) {
}

bool Broadcaster::PopMessageFromQueue(queue_entry_t& msg)
{
    if (m_msg_queue.empty())
    {
        return false;
    }

    msg = m_msg_queue.front();
    m_msg_queue.pop_front();

    if (msg.type == RoRnet::MSG2_STREAM_DATA_DISCARDABLE)
    {
        msg.type = RoRnet::MSG2_STREAM_DATA;
    }

    return true;
}

void Broadcaster::QueueMessage(int type, int uid, unsigned int streamid, unsigned int len, const char *data) {

    queue_entry_t msg = {type, uid, streamid, len, ""};
    memcpy(msg.data, data, len);

    { // USELESS BLOCK
        if (m_msg_queue.empty()) {
            m_packet_drop_counter = 0;
            m_is_dropping_packets = (++m_packet_good_counter > 3) ? false : m_is_dropping_packets;
        } else if (type == RoRnet::MSG2_STREAM_DATA_DISCARDABLE) {
            auto search = std::find_if(m_msg_queue.begin(), m_msg_queue.end(), [&](const queue_entry_t& m)
                    { return m.type == RoRnet::MSG2_STREAM_DATA_DISCARDABLE && m.uid == uid && m.streamid == streamid; });
            if (search != m_msg_queue.end()) {
                // Found outdated discardable streamdata -> replace it
                (*search) = msg;
                m_packet_good_counter = 0;
                m_is_dropping_packets = (++m_packet_drop_counter > 3) ? true : m_is_dropping_packets;
                Messaging::StatsAddOutgoingDrop(sizeof(RoRnet::Header) + msg.datalen); // Statistics
                return;
            }
        }
        m_msg_queue.push_back(msg);
    }
}
