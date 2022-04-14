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
along with "Rigs of Rods Server". 
If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "sequencer.h"
#include "prerequisites.h"

namespace Messaging {

    int SWSendMessage(
            SWInetSocket *socket,
            int msg_type,
            int msg_client_id,
            unsigned int msg_stream_id,
            unsigned int payload_len,
            const char *payload);

    int ReceiveMessage(
            SWInetSocket *socket,
            int *out_msg_type,
            int *out_client_id,
            unsigned int *out_stream_id,
            unsigned int *out_payload_len,
            char *out_payload,
            unsigned int payload_buf_len);

    int broadcastLAN();

    void StatsAddIncoming(int bytes);

    void StatsAddIncomingDrop(int bytes);

    void StatsAddOutgoingDrop(int bytes);

    stream_traffic_t GetTrafficStats();

    void UpdateMinuteStats();

    int getTime();

} // namespace Messaging
