#!/bin/sh

# Modified version of the runtests.sh that is distributed with lua-cjson.

PLATFORM="`uname -s`"
[ "$1" ] && VERSION="$1" || VERSION="2.1.0"

set -e

# Portable "ggrep -A" replacement.
contextgrep() {
    awk "/$1/ { count = ($2 + 1) } count > 0 { count--; print }"
}

do_tests() {
    echo
    cd tests
    luasetup="package.cpath=\"`pwd`/?.so\" ; print(\"Testing Lua CJSON version \" .. require(\"cjson\")._VERSION)"
    echo $luasetup
    lua -e "$luasetup"
    ./test.lua | contextgrep 'FAIL|Summary' 3 | grep -v PASS | cut -c -150
    cd ..
}

echo "===== Building UTF-8 test data ====="
( cd tests && ./genutf8.pl; )

echo "===== Cleaning old build data ====="
make clean
rm -f tests/cjson.so

echo "===== Testing Makefile build ====="
./build-rosie-macosx.sh
cp -r lua/cjson cjson.so tests
do_tests
rm -rf tests/cjson{,.so}

echo ""
echo "(Removing generated utf8 test data)"
rm -f tests/utf8.dat

