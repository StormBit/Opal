#include "EventBus.h"

#include <string.h>

using namespace bot;
using namespace std;

void EventBus::Value::wrap(lua_State *L) const
{
    switch (type()) {
    case Type::Nil:
        lua_pushnil(L);
        break;
    case Type::Number:
        lua_pushnumber(L, number());
        break;
    case Type::String:
        lua_pushlstring(L, string().c_str(), string().size());
        break;
    case Type::Bool:
        lua_pushboolean(L, boolean());
        break;
    case Type::Table:
        {
            const Value::Table &t = table();
            lua_createtable(L, 0, t.size());
            int idx = lua_gettop(L);
            for (auto &kv : t) {
                kv.first.wrap(L);
                kv.second.wrap(L);
                lua_settable(L, idx);
            }
            break;
        }
    case Type::OwnedObject:
        assert(!"Cannot copy owned object");
    case Type::RefObject:
        {
            LuaWrap<IObject>::create(L, ref_object_);
            break;
        }
    }
}

EventBus::Value EventBus::Value::unwrap(lua_State *L)
{
    int vtype = lua_type(L, -1);
    Value value;
    switch (vtype) {
    case LUA_TNONE:
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        value = Value(lua_tonumber(L, -1));
        break;
    case LUA_TBOOLEAN:
        value = Value((bool)lua_toboolean(L, -1));
        break;
    case LUA_TSTRING:
        value = Value(::string(lua_tostring(L, -1)));
        break;
    case LUA_TTABLE:
        luaL_error(L, "NYI");
        break;
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
    default:
        luaL_error(L, "Unsupported type %s", lua_typename(L, -1));
        break;
    }
    return value;
}

string EventBus::Value::to_string(size_t indent) const
{
    switch (type()) {
    case Type::Nil:
        return "nil";
    case Type::Number:
        return ::to_string(number());
    case Type::String:
        return "\"" + string() + "\"";
    case Type::Bool:
        return boolean()? "true" : "false";
    case Type::Table:
        {
            ::string str;
            ::string outer(indent * 2, ' ');
            ::string inner((indent+1) * 2, ' ');
            str.append("{");
            for (auto &kv : table()) {
                str.append(inner);
                str.append(kv.first.to_string(indent+1));
                str.append(": ");
                str.append(kv.second.to_string(indent+1));
                str.append("\n");
            }
            str.append(outer);
            str.append("}");
            return str;
        }
    case Type::OwnedObject:
        {
            char buf[64];
            sprintf(buf, "owned %s @%p", owned_object_->tname(), (void*)&*owned_object_);
            return buf;
        }
    case Type::RefObject:
        {
            char buf[64];
            sprintf(buf, "ref %s @%p", ref_object_->tname(), (void*)ref_object_);
            return buf;
        }
    }
}

void EventBus::Value::print() const
{
    printf("%s\n", to_string().c_str());
}

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
            val.wrap(L);
            int res = lua_pcall(L, 1, 0, idx);
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
    if (lua_gettop(L) != 3) {
        luaL_error(L, "Invalid number of parameters");
        return 0;
    }
    lua_pushvalue(L, 3);
    bus.fire(name, EventBus::Value::unwrap(L));
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
