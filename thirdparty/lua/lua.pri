# Lua 5.4.7 (https://www.lua.org, MIT license, see the end of lua.h) without its interpreters. Built are the core
# and the libraries the source scripts get (onelua.cpp), as C++; not the io, os, package, debug and coroutine
# libraries.
INCLUDEPATH += $$PWD

SOURCES += $$PWD/onelua.cpp
