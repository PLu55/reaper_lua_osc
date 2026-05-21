// reaper_liblo.c
// Minimal liblo binding for Lua 5.3 (REAPER ReaScript Lua)
// Features:
//   - server_new(port)   — non-threaded; poll with server_recv_noblock
//   - server_free(srv)
//   - server_recv_noblock(srv [, timeout_ms])
//   - server_thread_new(port)
//   - server_thread_start(st), server_thread_stop(st), server_thread_free(st)
//   - add_method(srv_or_st, path, types, lua_callback)
//   - send(host, port, path, types, ...)

#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <lo/lo.h>

typedef struct
{
    lo_server_thread st; /* may be NULL when using non-threaded server */
    lo_server srv;       /* may be NULL when using threaded server */
    lua_State *L;        /* main Lua state (REAPER) */
    int cb_ref;          /* registry ref to callback function */
} method_ud;

typedef struct
{
    lo_server_thread st;
    lua_State *L;
} st_ud;

/* Non-threaded server userdata */
typedef struct
{
    lo_server srv;
    lua_State *L;
} srv_ud;

// ------------ helpers ------------

static void push_argv(lua_State *L, const char *types, lo_arg **argv, int argc)
{
    for (int i = 0; i < argc; i++)
    {
        char t = types[i];
        switch (t)
        {
        case 'i':
            lua_pushinteger(L, argv[i]->i);
            break;
        case 'h':
            lua_pushinteger(L, argv[i]->h);
            break;
        case 'f':
            lua_pushnumber(L, argv[i]->f);
            break;
        case 'd':
            lua_pushnumber(L, argv[i]->d);
            break;
        case 's':
            lua_pushstring(L, &argv[i]->s);
            break;
        case 'c':
        {
            char tmp[2] = {(char)argv[i]->c, 0};
            lua_pushstring(L, tmp);
            break;
        }
        case 'T':
            lua_pushboolean(L, 1);
            break;
        case 'F':
            lua_pushboolean(L, 0);
            break;
        case 'N':
            lua_pushnil(L);
            break;
        default:
            lua_pushnil(L); // unsupported type → nil
            break;
        }
    }
}

// ------------ liblo handler ------------
// Called on liblo's server thread, NOT the REAPER main thread.
// IMPORTANT: REAPER API is not thread-safe. We therefore only call Lua callback,
// which must NOT call reaper.* directly. The Lua side should queue work and execute
// it via reaper.defer on the main thread.
/*
static int lo_handler(const char *path, const char *types, lo_arg **argv,
                      int argc, lo_message msg, void *user_data)
{
    method_ud *mud = (method_ud *)user_data;
    lua_State *L = mud->L;

    // Call Lua callback: cb(path, types, argc, ...)
    lua_rawgeti(L, LUA_REGISTRYINDEX, mud->cb_ref);
    lua_pushstring(L, path);
    lua_pushstring(L, types ? types : "");
    lua_pushinteger(L, argc);
    push_argv(L, types ? types : "", argv, argc);

    // Protected call
    int nargs = 3 + argc;
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK)
    {
        // We cannot safely print with REAPER here; just swallow error.
        lua_pop(L, 1);
    }
    return 0;
}
*/

static int lo_handler(const char *path, const char *types, lo_arg **argv,
                      int argc, lo_message msg, void *user_data)
{
    method_ud *mud = (method_ud *)user_data;
    lua_State *L = mud->L;

    // Source address
    const char *src_host = "";
    const char *src_port = "";
    lo_address src = lo_message_get_source(msg);
    if (src)
    {
        const char *h = lo_address_get_hostname(src);
        const char *p = lo_address_get_port(src);
        if (h)
            src_host = h;
        if (p)
            src_port = p;
    }

    // Call Lua callback:
    // cb(path, types, src_host, src_port, argc, ...)
    lua_rawgeti(L, LUA_REGISTRYINDEX, mud->cb_ref);
    lua_pushstring(L, path);
    lua_pushstring(L, types ? types : "");
    lua_pushstring(L, src_host);
    lua_pushstring(L, src_port);
    lua_pushinteger(L, argc);
    push_argv(L, types ? types : "", argv, argc);

    int nargs = 5 + argc;
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK)
    {
        lua_pop(L, 1); // swallow error
    }
    return 0;
}

// ------------ server thread userdata ------------

static st_ud *check_st(lua_State *L, int idx)
{
    return (st_ud *)luaL_checkudata(L, idx, "reaper_liblo.server_thread");
}

static int l_server_thread_new(lua_State *L)
{
    const char *port = luaL_checkstring(L, 1);
    st_ud *ud = (st_ud *)lua_newuserdata(L, sizeof(st_ud));
    memset(ud, 0, sizeof(*ud));
    ud->L = L;
    ud->st = lo_server_thread_new(port, NULL);
    if (!ud->st)
        return luaL_error(L, "lo_server_thread_new failed (port %s)", port);

    luaL_getmetatable(L, "reaper_liblo.server_thread");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_server_thread_start(lua_State *L)
{
    st_ud *ud = check_st(L, 1);
    lo_server_thread_start(ud->st);
    return 0;
}

static int l_server_thread_stop(lua_State *L)
{
    st_ud *ud = check_st(L, 1);
    lo_server_thread_stop(ud->st);
    return 0;
}

static int l_server_thread_free(lua_State *L)
{
    st_ud *ud = check_st(L, 1);
    if (ud->st)
    {
        lo_server_thread_free(ud->st);
        ud->st = NULL;
    }
    return 0;
}

// ------------ non-threaded server userdata ------------

static srv_ud *check_srv(lua_State *L, int idx)
{
    return (srv_ud *)luaL_checkudata(L, idx, "reaper_liblo.server");
}

static int l_server_new(lua_State *L)
{
    const char *port = luaL_checkstring(L, 1);
    srv_ud *ud = (srv_ud *)lua_newuserdata(L, sizeof(srv_ud));
    memset(ud, 0, sizeof(*ud));
    ud->L = L;
    ud->srv = lo_server_new(port, NULL);
    if (!ud->srv)
        return luaL_error(L, "lo_server_new failed (port %s)", port);

    luaL_getmetatable(L, "reaper_liblo.server");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_server_free(lua_State *L)
{
    srv_ud *ud = check_srv(L, 1);
    if (ud->srv)
    {
        lo_server_free(ud->srv);
        ud->srv = NULL;
    }
    return 0;
}

static int l_server_recv_noblock(lua_State *L)
{
    srv_ud *ud = check_srv(L, 1);
    int timeout_ms = (int)luaL_optinteger(L, 2, 0);
    if (!ud->srv)
        return luaL_error(L, "server_recv_noblock: server already freed");
    int rc = lo_server_recv_noblock(ud->srv, timeout_ms);
    lua_pushinteger(L, rc);
    return 1;
}

// ------------ method registration ------------

static int l_add_method(lua_State *L)
{
    /* Accept either a server_thread or a plain server userdata */
    st_ud *st = (st_ud *)luaL_testudata(L, 1, "reaper_liblo.server_thread");
    srv_ud *sv = (st == NULL)
                     ? (srv_ud *)luaL_testudata(L, 1, "reaper_liblo.server")
                     : NULL;

    if (!st && !sv)
        return luaL_argerror(L, 1, "expected server_thread or server userdata");

    const char *path = luaL_checkstring(L, 2);
    const char *types = luaL_optstring(L, 3, NULL);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    method_ud *mud = (method_ud *)malloc(sizeof(method_ud));
    memset(mud, 0, sizeof(*mud));
    mud->L = st ? st->L : sv->L;

    lua_pushvalue(L, 4);
    mud->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (st)
    {
        mud->st = st->st;
        lo_server_thread_add_method(st->st, path, types, lo_handler, mud);
    }
    else
    {
        mud->srv = sv->srv;
        lo_server_add_method(sv->srv, path, types, lo_handler, mud);
    }

    return 0;
}

// ------------ send ------------

static int l_send(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    const char *port = luaL_checkstring(L, 2);
    const char *path = luaL_checkstring(L, 3);
    const char *types = luaL_checkstring(L, 4);

    lo_address a = lo_address_new(host, port);
    if (!a)
        return luaL_error(L, "lo_address_new failed");

    lo_message m = lo_message_new();

    int n = lua_gettop(L);
    int expected = (int)strlen(types);
    if (n < 4 + expected)
    {
        lo_message_free(m);
        lo_address_free(a);
        return luaL_error(L, "send: expected %d args for types '%s'", expected, types);
    }

    for (int i = 0; i < expected; i++)
    {
        char t = types[i];
        int argi = 5 + i;
        switch (t)
        {
        case 'i':
            lo_message_add_int32(m, (int32_t)luaL_checkinteger(L, argi));
            break;
        case 'h':
            lo_message_add_int64(m, (int64_t)luaL_checkinteger(L, argi));
            break;
        case 'f':
            lo_message_add_float(m, (float)luaL_checknumber(L, argi));
            break;
        case 'd':
            lo_message_add_double(m, (double)luaL_checknumber(L, argi));
            break;
        case 's':
            lo_message_add_string(m, luaL_checkstring(L, argi));
            break;
        case 'T':
            lo_message_add_true(m);
            break;
        case 'F':
            lo_message_add_false(m);
            break;
        case 'N':
            lo_message_add_nil(m);
            break;
        default:
            lo_message_free(m);
            lo_address_free(a);
            return luaL_error(L, "send: unsupported type '%c'", t);
        }
    }

    int rc = lo_send_message(a, path, m);

    lo_message_free(m);
    lo_address_free(a);

    lua_pushinteger(L, rc);
    return 1;
}

// ------------ metatable / module ------------

static int l_gc_server_thread(lua_State *L)
{
    return l_server_thread_free(L);
}

static int l_gc_server(lua_State *L)
{
    return l_server_free(L);
}

int luaopen_reaper_liblo(lua_State *L)
{
    // server_thread metatable
    luaL_newmetatable(L, "reaper_liblo.server_thread");
    lua_pushcfunction(L, l_gc_server_thread);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // non-threaded server metatable
    luaL_newmetatable(L, "reaper_liblo.server");
    lua_pushcfunction(L, l_gc_server);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // module table
    lua_newtable(L);
    /* non-threaded server (main-thread polling) */
    lua_pushcfunction(L, l_server_new);
    lua_setfield(L, -2, "server_new");
    lua_pushcfunction(L, l_server_free);
    lua_setfield(L, -2, "server_free");
    lua_pushcfunction(L, l_server_recv_noblock);
    lua_setfield(L, -2, "server_recv_noblock");
    /* threaded server */
    lua_pushcfunction(L, l_server_thread_new);
    lua_setfield(L, -2, "server_thread_new");
    lua_pushcfunction(L, l_server_thread_start);
    lua_setfield(L, -2, "server_thread_start");
    lua_pushcfunction(L, l_server_thread_stop);
    lua_setfield(L, -2, "server_thread_stop");
    lua_pushcfunction(L, l_server_thread_free);
    lua_setfield(L, -2, "server_thread_free");
    /* shared */
    lua_pushcfunction(L, l_add_method);
    lua_setfield(L, -2, "add_method");
    lua_pushcfunction(L, l_send);
    lua_setfield(L, -2, "send");
    return 1;
}
