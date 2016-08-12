# Rigs of Rods Server 

|  Build status 	|                                                                                                                                                                         	|
|---------------	|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------	|
| Linux:        	| [![Build Status](https://travis-ci.org/RigsOfRods/ror-server.png?branch=master)](https://travis-ci.org/RigsOfRods/ror-server)                                           	|
| Windows:      	| [![appveyor build Status](https://ci.appveyor.com/api/projects/status/github/RigsOfRods/ror-server?svg=true)](https://ci.appveyor.com/project/AnotherFoxGuy/ror-server) 	|

## DESCRIPTION

The Rigs of Rods Server is a game server for Rigs of Rods (http://www.rigsofrods.com/).
It is compabible with RoR clients version 0.33d, and any future client using the RORNET protocol version 2.0

## INSTALLATION

please refer to http://wiki.rigsofrods.com/pages/Compiling_Server

## USAGE

to start a server:
rorserver (-ip <public IP>) (-port <port>) (-name <server name>) (-terrain <terrain name>) (-maxclients <max number of clients>) (-debug) (-lan) (-inet)

## CLI arguments:
```
-lan
	Force LAN mode: will not try to connect to the central repository to register the server.
-inet
	Force Internet mode. Will fail if it can not register with the central repository.
-ip <public IP>
	The IP address to which the server can be joined. Optional in inet mode. Mandatory in lan mode.
-port <port>
	The port number used by the server. Any port is possible, traditionnally it is 12000 and upward.
	Example: -port 12000
-name <server name>
	The advertised name of the server. No spaces allowed.
	Example: -name Bobs_place
-terrain <terrain name>
	The terrain served by this server. Must be a valid RoR terrain name, without the .terrn extension.
	Example: -terrain nhelens
-maxclients <max number of clients>
	The maximum number of clients handled by this server. Values below 16 are preferable. See the bandwidth note below.
-debug
	Verbose output
```

Notes:
By default, if neither -lan nor -inet is used the server will try to register at the master server and fall back to LAN mode in case it fails.

## Bandwidth used by the server:
The RoR server uses large amounts of bandwidth. The general formula to compute bandwidth is:  
-DOWNLOAD: maxclients * 64kbit/s  
-UPLOAD  : maxclients * (maxclients-1) * 64kbit/s  
That translates into:  
-4 clients: 256kbit/s download, 768kbit/s upload <= will nearly saturate most ADSL links  
-8 clients: 512kbit/s download, 3.5Mbit/s upload  
-16 clients:  1Mbit/s download,  15Mbit/s upload  
-32 clients:  2Mbit/s download,  64Mbit/s upload <= will nearly saturate a 100Mbit/s LAN!

## LICENSE AND CREDITS

Copyright 2007  Pierre-Michel Ricordel and Thomas Fischer
Copyright 2016+ Rigs of Rods community

```
"Rigs of Rods Server" is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.
```

