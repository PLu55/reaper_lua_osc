#include "reaper_lua_osc.h"

#ifndef REAPER_LUA_OSC_VERSION
#define REAPER_LUA_OSC_VERSION "0.1.0"
#endif

const char *reaper_lua_osc_version(void)
{
    return REAPER_LUA_OSC_VERSION;
}

int reaper_lua_osc_add(int a, int b)
{
    return a + b;
}
