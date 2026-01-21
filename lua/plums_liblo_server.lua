-- plums_liblo_server.lua (with source capture + tx/ack replies)
-- plums_liblo_server.lua
-- Starts an OSC server using liblo via reaper_liblo.so
-- Receives JSON commands and executes them safely on REAPER main thread.

local reaper = _G.reaper
if not reaper then
    error("This script must be run inside REAPER (global 'reaper' API table not found).")
end

local MODULE_DIR         = reaper.GetResourcePath() .. "/Scripts/plums/core/?.so"
package.cpath            = package.cpath .. ";" .. MODULE_DIR

local lo                 = require("reaper_liblo")
local json               = require("dkjson")

local LISTEN_PORT        = "9002"
local SEND_PORT          = "9003"
local DEFAULT_REPLY_PATH = "/rpr"

-- Queue of { json_str=..., src_host=..., src_port=... }
local q                  = {}
local q_head, q_tail     = 1, 0

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

-- ---- Reply helpers (main thread only) ----

local function send_reply(host, port, path, tbl)
    local s = json.encode(tbl)
    -- types "s": one string argument
    lo.send(host, port, path, "s", s)
end

local function resolve_reply_target(msg, src_host, src_port)
    local host = msg.reply_host or src_host
    local port = msg.reply_port or src_port
    local path = msg.reply_path or DEFAULT_REPLY_PATH
    return host, port, path
end

-- ---- Command handlers (main thread) ----
-- Must return optional "result" table.

local function cmd_ping(_)
    reaper.ShowConsoleMsg("ping!")
    return { message = "pong" }
end

local function cmd_create_tracks(msg)
    local n = tonumber(msg.count) or 0
    if n <= 0 then error("count must be > 0") end
    for i = 1, n do
        reaper.InsertTrackAtIndex(i - 1, true)
    end
    reaper.TrackList_AdjustWindows(false)
    return { created = n }
end

local COMMANDS = {
    ping = cmd_ping,
    create_tracks = cmd_create_tracks,
}

-- ---- OSC callback (runs on liblo server thread) ----
-- Signature from C binding:
-- on_osc(path, types, src_host, src_port, argc, json_str)
-- DO NOT call reaper.* here.
local function on_osc(_path, _types, src_host, src_port, _argc, json_str)
    if type(json_str) ~= "string" then return end
    q_push({
        json_str = json_str,
        src_host = src_host or "",
        src_port = src_port or "",
    })
end

-- ---- Main thread loop ----
local function process_queue()
    local item = q_pop()
    while item do
        local msg, _, err = json.decode(item.json_str)
        if not msg then
            -- Can't decode; reply to sender if we have address
            -- if item.src_host ~= "" and item.src_port ~= "" then
            if true then
                send_reply("127.0.0.1", SEND_PORT, DEFAULT_REPLY_PATH, {
                    ok = false,
                    tx = nil,
                    error = "JSON decode error: " .. tostring(err),
                })
            end
        else
            local tx = msg.tx
            local cmd = msg.cmd
            local fn = cmd and COMMANDS[cmd]

            local host, port, path = resolve_reply_target(msg, item.src_host, item.src_port)

            if not (host and port and host ~= "" and port ~= "") then
                -- No place to reply; still execute
                host, port, path = nil, nil, nil
            end

            if not fn then
                if host then
                    send_reply(host, port, path, {
                        ok = false,
                        tx = tx,
                        error = "unknown cmd: " .. tostring(cmd),
                    })
                end
            else
                local ok, result_or_err = pcall(fn, msg)
                if ok then
                    if host then
                        send_reply(host, port, path, {
                            ok = true,
                            tx = tx,
                            result = result_or_err,
                        })
                    end
                else
                    if host then
                        send_reply(host, port, path, {
                            ok = false,
                            tx = tx,
                            error = tostring(result_or_err),
                        })
                    end
                end
            end
        end

        item = q_pop()
    end

    reaper.defer(process_queue)
end

-- ---- Start server thread ----
local st = lo.server_thread_new(LISTEN_PORT)
lo.add_method(st, "/plums/dispatch", "s", on_osc)
lo.server_thread_start(st)

reaper.ClearConsole()
reaper.ShowConsoleMsg("plums: liblo OSC server listening on 127.0.0.1:" .. LISTEN_PORT .. "\n")
reaper.ShowConsoleMsg("plums: replies default to sender via " .. DEFAULT_REPLY_PATH .. " (JSON string)\n")

process_queue()
