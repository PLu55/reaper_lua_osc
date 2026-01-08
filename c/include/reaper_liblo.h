#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <lua.h>

    // Lua module entrypoint. Returns a module table on the Lua stack.
    int luaopen_reaper_liblo(lua_State *L);

#ifdef __cplusplus
}
#endif
