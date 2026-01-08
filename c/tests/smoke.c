#include <stdio.h>

#include "reaper_lua_osc.h"

int main(void)
{
    printf("reaper_lua_osc version: %s\n", reaper_lua_osc_version());
    printf("2 + 3 = %d\n", reaper_lua_osc_add(2, 3));
    return 0;
}
