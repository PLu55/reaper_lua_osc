#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // Returns a static version string for the library.
    const char *reaper_lua_osc_version(void);

    // Example function: add two ints.
    int reaper_lua_osc_add(int a, int b);

#ifdef __cplusplus
}
#endif
