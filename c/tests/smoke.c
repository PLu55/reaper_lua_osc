#include <stdio.h>

#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "reaper_liblo.h"

int main(void)
{
    lua_State *L = luaL_newstate();
    if (!L)
    {
        fprintf(stderr, "Failed to create Lua state\n");
        return 1;
    }

    luaL_openlibs(L);

    // Push module table on stack
    if (luaopen_reaper_liblo(L) != 1)
    {
        fprintf(stderr, "luaopen_reaper_liblo did not return a module table\n");
        lua_close(L);
        return 1;
    }

    if (!lua_istable(L, -1))
    {
        fprintf(stderr, "reaper_liblo module return is not a table\n");
        lua_close(L);
        return 1;
    }

    lua_getfield(L, -1, "send");
    printf("reaper_liblo has send(): %s\n", lua_isfunction(L, -1) ? "yes" : "no");
    lua_pop(L, 2);

    lua_close(L);
    return 0;
}
