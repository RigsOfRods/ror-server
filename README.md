# Rigs of Rods Server 

|  Build status 	|                                                                                                                                                                             	|
|---------------	|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------	|
| Linux:        	| [![travis build Status](https://img.shields.io/travis/RigsOfRods/ror-server.svg?style=flat-square)](https://travis-ci.org/RigsOfRods/ror-server)                            	|
| Windows:      	| [![appveyor build Status](https://img.shields.io/appveyor/ci/AnotherFoxGuy/ror-server.svg?style=flat-square)](https://ci.appveyor.com/project/AnotherFoxGuy/ror-server)    	|

## Description

The Rigs of Rods Server is a game server for Rigs of Rods (https://www.rigsofrods.org/).
It is compabible with RoR clients version 0.4.8.0+, and any future client using the RoRNet protocol version 2.40+

## Installation

Please refer to: [Multiplayer server setup](http://docs.rigsofrods.org/gameplay/multiplayer-server-setup)

## Usage

To start a server using CLI arguments:

`rorserver` `-name <server name>` `-port <port>` `-terrain <terrain name>` `-maxclients <max number of clients>`

To start a server using a config file:

`rorserver` `-c your-config.cfg`

## CLI arguments:

```
-lan
	Force LAN mode: will not try to connect to the central repository to register the server.
-inet
	Force Internet mode. Will fail and exit if it can not register with the central repository.
-ip <public IP>
	The IP address to which the server can be joined. Optional in inet mode, mandatory in lan mode.
-port <port>
	The port number used by the server. Any port is possible, traditionnally it is 12000 and upward.
	Example: -port 12000
-name <server name>
	The advertised name of the server. No spaces allowed.
	Example: -name Bobs_place
-terrain <terrain name>
	The terrain served by this server. Must be a valid RoR terrain name, without the .terrn2 extension.
	Use `any` to allow the player to choose the terrain.
	Example: -terrain nhelens
-maxclients <max number of clients>
	The maximum number of clients handled by this server. Values below 16 are preferable. See the bandwidth note below.
```

## Config file parameters:

```
## amount of slots that clients can connect to
slots  = 10

## Server name, use _ instead of spaces
name   = Private_Server

## scriptname to use, only use if compiled with scripting support
# scriptname = foobar.as

## terrain to use. any lets the user select one
## example values: any, smallisland, island, aspen, nhelens, ...
terrain = any

## if we want to use the webserver, if we compiled support for it.
webserver = n

## port that the webserver will listen on. default it game port + 100
# webserverport = 14100

## the server password
# password = secret

## the IP the server will listen on, by default: any
# ip = 127.0.0.1

## port the server will listen on, by default random port
# port = 14000

## server mode. either inet or lan
mode = inet

## The maximum amount of vehicles a player is allowed to have
## Vehicles, i.e. loads, trailers, planes, cars, trucks, boats, etc.
## syntax: vehicles = <number greater than 0>
# vehiclelimit = 5

## The maximum rate (num. spawns per interval) of spawning new vehicles.
## Default: 0 = not limited
# vehicle-max-spawn-rate = 

## The time interval (in seconds) to evaluate max. spawn rate of new vehicles.
## Default: 0 = spawn rate not limited
# vehicle-spawn-interval =

## The location of the message of the day file
## syntax: motdfile = <path-to-file>
motdfile = /etc/rorserver/simple.motd

## The location of the authorizations file
## syntax: authfile = <path-to-file>
authfile = /etc/rorserver/simple.auth

## The location of the rules file for this server
## syntax: rulesfile = <path-to-file>
rulesfile = /etc/rorserver/simple.rules

## The owner of this server. This can be you, your organization or anything else.
## syntax: owner = <name|organisation>
owner = 

## The official website of this server.
## syntax: website = <URL>
website = 

## The IRC (Internet Relay Chat) url, port and channel for this server
## Warning: Do not use #. That will not work.
## syntax: irc = <your IRC info>
irc = 

## The VoIP info for this server (e.g. teamspeak, ventrilo, etc)
## syntax: voip = <your VoIP info>
voip =

## if we want to print the player table in the log
printstats = n

## if we want to run the server in the foreground. NOT RECOMMENDED
## unless you want to debug things
foreground = n

## Verbosity of the displayed LOG in foreground mode. 0 = highest, 5 = lowest. Only used when foreground=y
verbosity = 1

## Verbosity of the server file LOG. 0 = highest, 5 = lowest
logverbosity = 1

## reource directory, normally set via cmdline by the init script.
# resdir = /usr/local/share/rorserver/

## filename for the log, usually this is set by the init script
# logfilename = /var/log/rorserver/server-1.log

## Debug: Master serverlist host. Enter IP or hostname, i.e. `ror.example.com` or `12.34.56.78`
# serverlist-host = multiplayer.rigsofrods.org

## Debug: Master serverlist path. Leave blank or enter path, i.e. `/xror/mp-portal`
# serverlist-path = 

## Debug: Time in seconds, default 60.
# heartbeat-interval =

## Spam filter: time span (in seconds) to check for repeated messages.
## Default: 0 = disables spam filter.
# spamfilter-msg-interval =

## Spam filter: minimum number of repeated messages (within defined interval) to consider spam and penalize the player.
## Default: 0 = disables spam filter.
# spamfilter-msg-count =

## Spam filter: Gag time awarded for each detected spam message.
## Default: 10 seconds.
# spamfilter-gag-duration = 10
```

Notes:
By default, if neither `-lan` nor `-inet` is used the server will try to register at the master server and fall back to LAN mode in case it fails.

## Chat spam filtering

The spam filter operates by caching player's chat messages for a defined period of time (see config file) and looking for repeated messages (see config file).
If spam is detected, the player is silenced for some time (see config file) and server responds (to each message) `"You are gagged. Time remaining: N seconds."`.
The spam detection is continuous, and for each new spam message, the gag time is increased by the configured value.
Finally, if player behaves, the gag lapses and server displays (upon first passing message) `"Your gag has expired. Chat nicely!"`.

## Vehicle spawn limits

Server can limit the total number of vehicles the player spawns, as well as the rate at which they spawn.
  Exceeding the limits results in auto-kick.

The maximum vehicle count is specified in config file or command line.
  Player receives informational messages about the limit and current usage.

The spawn rate is specified in config file as a time interval and maximum number of spawns within the interval.
  When player uses 70% of the limit they begin getting warning messages.

## Bandwidth used by the server:
The RoR server uses large amounts of bandwidth. The general formula to compute bandwidth is:  

-DOWNLOAD: `maxclients * 64kbit/s` 

-UPLOAD  : `maxclients * (maxclients-1) * 64kbit/s` 

That translates into:  

-4 clients: `256kbit/s` download, `768kbit/s` upload <= will nearly saturate most ADSL links  
-8 clients: `512kbit/s` download, `3.5Mbit/s` upload  
-16 clients:  `1Mbit/s` download,  `15Mbit/s` upload  
-32 clients:  `2Mbit/s` download,  `64Mbit/s` upload <= will nearly saturate a 100Mbit/s LAN!

## License and Credits

Copyright 2007  Pierre-Michel Ricordel and Thomas Fischer

Copyright 2018+ Rigs of Rods Community

```
"Rigs of Rods Server" is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.
```

