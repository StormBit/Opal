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
    for (auto bus : lua_buses) {
        bus->fire(name, val);
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
    for (auto &event : handlers) {
        printf("%s:", event.first.c_str());
        for (auto &fn : event.second) {
            printf(" %p", (void*)&fn);
        }
        printf("\n");
    }
    printf("Buses:\n");
    for (auto bus : lua_buses) {
        printf("%p\n", bus);
    }
}

void EventBus::addBus(LuaBus &bus)
{
    lua_buses.push_back(&bus);
}

void LuaBus::openlib(lua_State *L)
{
    wrap_object(this, L);
    lua_setglobal(L, "eventbus");
}

void LuaBus::print()
{
    for (auto &event : lua_handlers) {
        printf("%s:", event.first.c_str());
        for (auto &ref : event.second) {
            ref.push();
            printf(" %p", lua_topointer(ref.getL(), -1));
            lua_pop(ref.getL(), 1);
        }
    }
}

void LuaBus::fire(const string &name, const Value &val)
{
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
            val.wrap(L);
            int res = lua_pcall(L, 1, 0, idx);
            if (res && lua_isstring(L, -1)) {
                const char *err = lua_tostring(L, -1);
                printf("in event '%s': %s\n", name.c_str(), err);
            }
        }
    }
}

void LuaBus::clear()
{
    for (auto &kv : lua_handlers) {
        for (auto &ref : kv.second) {
            ref.unref();
        }
    }
    lua_handlers.clear();
}

int LuaBus::lua_hook(lua_State *L)
{
    LuaBus &bus = unwrap(L, 1);
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

int LuaBus::lua_fire(lua_State *L)
{
    LuaBus &bus = unwrap(L, 1);
    const char *name = luaL_checkstring(L, 2);
    if (lua_gettop(L) != 3) {
        luaL_error(L, "Invalid number of parameters");
        return 0;
    }
    lua_pushvalue(L, 3);
    bus.fire(name, Value::unwrap(L));
    return 0;
}

int LuaBus::__index(lua_State *L)
{
    if (lua_gettop(L) != 2 || !lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
        lua_error(L);
        return 0;
    }
    const char *key = lua_tostring(L, 2);
    if (!strcmp(key, "hook")) {
        lua_pushcfunction(L, &LuaBus::lua_hook);
        return 1;
    }
    if (!strcmp(key, "fire")) {
        lua_pushcfunction(L, &LuaBus::lua_fire);
        return 1;
    }

    return 0;
}
