#ifndef BOT_LUA_MODULE_H
#define BOT_LUA_MODULE_H

#include <string>
#include <lua.hpp>
#include "LuaBridge/LuaBridge.h"

namespace bot {

class LuaModule {
public:
    LuaModule(std::string &&name) : name(name) {}

    void load();
    template<class T>
    void openlib(T &val) {
        val.openlib(L);
    }

private:
    std::string name;
    lua_State *L = nullptr;
};

}

#endif
