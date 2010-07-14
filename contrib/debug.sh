#!/bin/sh
gdb /usr/bin/rorserver -ex "r -webserver -name Test_Server -terrain aspen -inet -verbosity 0 -fg; bt"
