# reaper_lua_osc (C library)

This directory contains a small C shared library built with CMake.

## Build

From the repo root:

```sh
cmake -S c -B c/build
cmake --build c/build
```

Outputs (Linux):

- `c/build/libreaper_lua_osc.so`
- `c/build/reaper_lua_osc_smoke` (optional test exe)

## Run smoke test

```sh
./c/build/reaper_lua_osc_smoke
```

## Disable smoke test

```sh
cmake -S c -B c/build -DREAPER_LUA_OSC_BUILD_TEST=OFF
cmake --build c/build
```
