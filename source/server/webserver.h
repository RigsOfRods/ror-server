#pragma once

#ifdef WITH_WEBSERVER
class Sequencer; // Forward decl...

int StartWebserver(int port, Sequencer *sequencer, bool is_advertised, int trust_level);

int StopWebserver();

#endif
