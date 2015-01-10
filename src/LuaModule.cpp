#include "LuaModule.h"

using namespace std;
using namespace bot;

void LuaModule::load()
{
    L = luaL_newstate();
    luaL_openlibs(L);
    auto path = "modules/" + name + ".lua";
    if (luaL_dofile(L, path.c_str())) {
        const char *str = lua_tostring(L, -1);
        printf("error loading module %s: %s\n", name.c_str(), str);
    }
}
