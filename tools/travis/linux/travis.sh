#!/bin/sh
#
# Build script for travis-ci.org builds to handle compiles 
# and static analysis when ANALYZE=true.
#
if [ $ANALYZE = "true" ]; then
    if [ "$CXX" = "clang++" ]; then
        scan-build cmake -G "Unix Makefiles" \
          -DCMAKE_INSTALL_PREFIX:STRING=/usr \
          -DRORSERVER_NO_STACKLOG:BOOL=ON \
          -DRORSERVER_CRASHHANDLER:BOOL=ON \
          -DRORSERVER_GUI:BOOL=ON \
          -DRORSERVER_WITH_ANGELSCRIPT:BOOL=ON \
          -DRORSERVER_WITH_WEBSERVER:BOOL=OFF \
          .
        scan-build \
          -enable-checker core \
          -enable-checker cplusplus \
          -enable-checker security \
          -enable-checker deadcode \
          -enable-checker unix \
          --status-bugs -v \
          make -j2
        exit 0
    else
        cppcheck --version
        cppcheck \
          --template "{file}({line}): {severity} ({id}): {message}" \
          --enable=information --enable=performance \
          --force --std=c++11 -j2 ./source \
          1> /dev/null 2> cppcheck.txt
        if [ -s cppcheck.txt ]; then
            cat cppcheck.txt
            exit 0
        fi
    fi
else # no static analysis, do regular build
    ./tools/travis/linux/build.sh
fi