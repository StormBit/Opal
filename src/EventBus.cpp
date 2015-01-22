#include "EventBus.h"

#include <string.h>

using namespace bot;
using namespace std;

void EventBus::fire(const string &name, const Value &val)
{
    auto it1 = handlers.find(name);
    if (it1 != handlers.end()) {
        for (auto &h : it1->second) {
            h(val);
        }
    }
    auto it2 = lua_handlers.find(name);
    if (it2 != lua_handlers.end()) {
        for (auto &h : it2->second) {
            lua_State *L = h.getL();
            lua_getglobal(L, "onerror");
            int idx = 0;
            if (lua_isfunction(L, -1)) {
                idx = lua_gettop(L);
            }
            h.push();
            luaL_checktype(L, -1, LUA_TFUNCTION);
            int res = lua_pcall(L, 0, 0, idx);
            if (res && lua_isstring(L, -1)) {
                const char *err = lua_tostring(L, -1);
                printf("in event '%s': %s\n", name.c_str(), err);
            }
        }
    }
}

void EventBus::hook(string &&name, function<void (const Value &val)> &&fn)
{
    auto res = handlers.emplace(piecewise_construct,
                                forward_as_tuple(name),
                                forward_as_tuple());
    res.first->second.emplace_back(fn);
}

void EventBus::print()
{
    printf("--- C++ ---\n");
    for (auto &event : handlers) {
        printf("%s:", event.first.c_str());
        for (auto &fn : event.second) {
            printf(" %p", (void*)&fn);
        }
        printf("\n");
    }
    printf("--- Lua ---\n");
    for (auto &event : lua_handlers) {
        printf("%s:", event.first.c_str());
        for (auto &ref : event.second) {
            ref.push();
            printf(" %p", lua_topointer(ref.getL(), -1));
            lua_pop(ref.getL(), 1);
        }
    }
}

void EventBus::openlib(lua_State *L)
{
    wrap(L);
    lua_setglobal(L, "eventbus");
}

LuaWrap<EventBus> &EventBus::wrap(lua_State *L)
{
    auto &w = LuaWrap<EventBus>::create(L, *this);
    if (luaL_newmetatable(L, "EventBus")) {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, &EventBus::__index);
        lua_settable(L, -3);
        LuaWrap<EventBus>::add_gc(L);
    }
    lua_setmetatable(L, -2);
    return w;
}

int EventBus::lua_hook(lua_State *L)
{
    EventBus &bus = reinterpret_cast<LuaWrap<EventBus>*>(luaL_checkudata(L, 1, "EventBus"))->get();
    const char *name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    if (lua_gettop(L) != 3) {
        luaL_error(L, "Invalid number of parameters");
        return 0;
    }

    auto it = bus.lua_handlers.emplace(name, std::vector<LuaRef>()).first;
    lua_pushvalue(L, 3);
    it->second.emplace_back(LuaRef::create(L));
    return 0;
}

int EventBus::lua_fire(lua_State *L)
{
    EventBus &bus = reinterpret_cast<LuaWrap<EventBus>*>(luaL_checkudata(L, 1, "EventBus"))->get();
    const char *name = luaL_checkstring(L, 2);
    int vtype = lua_type(L, 3);
    Value value;
    switch (vtype) {
    case LUA_TNONE:
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        value = Value(lua_tonumber(L, 3));
        break;
    case LUA_TBOOLEAN:
        value = Value((bool)lua_toboolean(L, 3));
        break;
    case LUA_TSTRING:
        value = Value(lua_tostring(L, 3));
        break;
    case LUA_TTABLE:
        luaL_error(L, "NYI");
        break;
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
    default:
        luaL_error(L, "Unsupported type %s", lua_typename(L, 3));
        break;
    }
    bus.fire(name, value);
    return 0;
}

int EventBus::__index(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        lua_error(L);
        return 0;
    }
    const char *key = lua_tostring(L, 2);
    if (!strcmp(key, "hook")) {
        lua_pushcfunction(L, &EventBus::lua_hook);
        return 1;
    }
    if (!strcmp(key, "fire")) {
        lua_pushcfunction(L, &EventBus::lua_fire);
        return 1;
    }

    return 0;
}
