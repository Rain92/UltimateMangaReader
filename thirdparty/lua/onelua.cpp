// Lua in one translation unit, like onelua.c of the Lua distribution, compiled as C++: its errors and yields
// unwind with exceptions instead of longjmp, so the destructors of the C++ host functions run. Include lua.h,
// lauxlib.h and lualib.h directly (not lua.hpp, whose extern "C" doesn't fit).
#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#include "luaconf.h"

// the internal symbols stay in here
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC static
#define LUAI_DDEC(def) /* empty */
#define LUAI_DDEF static

// core
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"

// auxiliary library
// GCC 14 sees an overflow in the memcpy() of luaL_addlstring() on 32-bit targets that the checks before it rule out
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wrestrict"
#include "lauxlib.c"
#pragma GCC diagnostic pop

// the libraries the scripts get (see luaengine.cpp)
#include "lbaselib.c"
#include "lmathlib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
