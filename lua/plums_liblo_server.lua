-- plums_liblo_server.lua
-- Starts an OSC server using liblo via reaper_liblo.so
-- Receives JSON commands and executes them safely on REAPER main thread.

local reaper = _G.reaper
if not reaper then
    error("This script must be run inside REAPER (global 'reaper' API table not found).")
end

local MODULE_DIR = reaper.GetResourcePath() .. "/Scripts/plums/core/?.so"
package.cpath = package.cpath .. ";" .. MODULE_DIR

local lo = require("reaper_liblo")
local json = require("dkjson")

local LISTEN_PORT = "9002"

-- Simple lock-free-ish queue (single-producer thread -> single-consumer main thread)
-- In practice: we still use a Lua table; this is control rate so fine.
local q = {}
local q_head, q_tail = 1, 0

local function q_push(item)
    q_tail = q_tail + 1
    q[q_tail] = item
end

local function q_pop()
    if q_head > q_tail then return nil end
    local item = q[q_head]
    q[q_head] = nil
    q_head = q_head + 1
    return item
end

-- ---- Command handlers (main thread!) ----

local function cmd_ping(msg)
    -- Example: reply path/port if provided in msg
    reaper.ShowConsoleMsg("plums: pong\n")
end

local function cmd_create_tracks(msg)
    local n = tonumber(msg.count) or 0
    if n <= 0 then error("count must be > 0") end
    for i = 1, n do
        reaper.InsertTrackAtIndex(i - 1, true)
    end
    reaper.TrackList_AdjustWindows(false)
end

local COMMANDS = {
    ping = cmd_ping,
    create_tracks = cmd_create_tracks,
}

-- ---- Decode + enqueue (OSC thread calls this via C callback) ----
-- DO NOT call reaper.* here.
local function on_osc(path, types, argc, json_str)
    -- We declared types "s", so json_str is first arg
    if type(json_str) ~= "string" then return end
    q_push(json_str)
end

-- Convenience endpoint: allow plain OSC /ping with no args.
-- This runs on the liblo server thread; we only enqueue work.
local function on_ping(path, types, argc)
    q_push('{"cmd":"ping"}')
end

-- ---- Main thread loop ----
local function process_queue()
    local item = q_pop()
    while item do
        local msg, _, err = json.decode(item)
        if not msg then
            reaper.ShowConsoleMsg("plums: JSON decode error: " .. tostring(err) .. "\n")
        else
            local cmd = msg.cmd
            local fn = COMMANDS[cmd]
            if not fn then
                reaper.ShowConsoleMsg("plums: unknown cmd: " .. tostring(cmd) .. "\n")
            else
                local ok, e = pcall(fn, msg)
                if not ok then
                    reaper.ShowConsoleMsg("plums: cmd error: " .. tostring(e) .. "\n")
                end
            end
        end
        item = q_pop()
    end

    reaper.defer(process_queue)
end

-- ---- Start server thread ----
local st = lo.server_thread_new(LISTEN_PORT)
-- Send JSON strings here, e.g. {"cmd":"create_tracks","count":4}
lo.add_method(st, "/plums/dispatch", "s", on_osc)
-- Plain ping (no args)
lo.add_method(st, "/ping", nil, on_ping)
lo.server_thread_start(st)

reaper.ShowConsoleMsg("plums: liblo OSC server listening on 127.0.0.1:" .. LISTEN_PORT .. "\n")

-- Keep running
process_queue()

-- Note: REAPER will GC the userdata on script stop; for a stop button you can add:
-- lo.server_thread_stop(st); lo.server_thread_free(st)
