#!/bin/bash
BASEDIR=${0%/*}
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BASEDIR/"
$BASEDIR/rorserver $@
