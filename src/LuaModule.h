#ifndef BOT_LUA_MODULE_H
#define BOT_LUA_MODULE_H

#include <string>
#include <lua.hpp>
#include <uv.h>

#include "LuaWrap.h"

namespace bot {

const char LuaModuleName[] = "LuaModule";

class LuaModule : public ObjectMixins<LuaModule, LuaModuleName> {
public:
    LuaModule(std::string &&name) : name(name) {}

    void load(uv_loop_t *loop);
    void run();
    template<class T>
    void openlib(T &val) {
        val.openlib(L);
    }
    lua_State *getL() {
        return L;
    }

    static LuaModule &getModule(lua_State *L);

    uv_loop_t *loop = nullptr;

private:
    std::string name;
    lua_State *L = nullptr;
    int chunk = 0;
};

}

#endif
