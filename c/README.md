# reaper_lua_osc (C library)

This directory contains C shared libraries built with CMake.

## Build

From the repo root:

```sh
cmake -S c -B c/build
cmake --build c/build
```

Outputs (Linux):

- `c/build/reaper_liblo.so` (Lua module for REAPER)
- `c/build/reaper_liblo_smoke` (optional test exe)
- `c/build/libreaper_lua_osc.so` + `c/build/reaper_lua_osc_smoke` (legacy demo)

## Run smoke test

```sh
./c/build/reaper_liblo_smoke
```

## Disable smoke test

```sh
cmake -S c -B c/build -DREAPER_LIBLO_BUILD_TEST=OFF
cmake --build c/build

## Make REAPER find the module

Your REAPER Lua script already appends this to `package.cpath`:

`~/.config/REAPER/Scripts/plums/core/?.so`

So `require("reaper_liblo")` will work once `reaper_liblo.so` is present in that folder.

You can deploy automatically during build:

```sh
cmake -S c -B c/build -DREAPER_LIBLO_DEPLOY_DIR="$HOME/.config/REAPER/Scripts/plums/core"
cmake --build c/build
```

```

## Simple test

```sh
oscsend osc.udp://localhost:9002 /plums/dispatch s '{"cmd":"ping"}'

```

Should type "plums: pong" in the REAPER console.
