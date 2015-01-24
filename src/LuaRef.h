#ifndef BOT_LUA_REF_H
#define BOT_LUA_REF_H

#include <lua.hpp>

namespace bot {

class LuaRef {
public:
    LuaRef() {}
    LuaRef(lua_State *L, int ref) : L(L), ref(ref) {}

    lua_State *getL() {
        return L;
    }

    static LuaRef create(lua_State *L) {
        return LuaRef(L, luaL_ref(L, LUA_REGISTRYINDEX));
    }

    void push() const {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    }

private:
    lua_State *L = nullptr;
    int ref = LUA_NOREF;
};

}

#endif
