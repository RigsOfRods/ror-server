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

#include "prerequisites.h"

///TODO: Remove this class, obsoleted by port to Libevent, only left here for nicer diff.
class Listener {
private:
    Sequencer *m_sequencer;
public:
    Listener(Sequencer *sequencer);

    void HandleNewConnection(kissnet::tcp_socket socket);
};

