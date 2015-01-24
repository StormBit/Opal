#include "LuaModule.h"

using namespace std;
using namespace bot;

void LuaModule::load(uv_loop_t *loop)
{
    this->loop = loop;
    L = luaL_newstate();
    lua_pushstring(L, "LuaModuleInstance");
    wrap_object(this, L);
    lua_rawset(L, LUA_REGISTRYINDEX);
    luaL_openlibs(L);
    auto path = "modules/" + name + ".lua";
    if (luaL_loadfile(L, path.c_str())) {
        const char *str = lua_tostring(L, -1);
        printf("error loading module %s: %s\n", name.c_str(), str);
    } else {
        chunk = luaL_ref(L, LUA_REGISTRYINDEX);
    }
}

void LuaModule::run()
{
    if (!chunk) {
        return;
    }
    lua_pushinteger(L, chunk);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_pcall(L, 0, 0, 0)) {
        const char *str = lua_tostring(L, -1);
        printf("error running module %s: %s\n", name.c_str(), str);
    }
}

LuaModule &LuaModule::getModule(lua_State *L)
{
    lua_pushstring(L, "LuaModuleInstance");
    lua_rawget(L, LUA_REGISTRYINDEX);
    return reinterpret_cast<LuaWrap<LuaModule>*>(luaL_checkudata(L, -1, LuaModuleName))->get();
}
