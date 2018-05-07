#!/bin/sh
find ../../source -iname "*.cpp"	| xargs uncrustify -l CPP -c rigs-of-rods.cfg --no-backup
find ../../source -iname "*.h"		| xargs uncrustify -l CPP -c rigs-of-rods.cfg --no-backup
