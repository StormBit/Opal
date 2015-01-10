#include "LuaModule.h"

using namespace std;
using namespace bot;
using namespace luabridge;

void LuaModule::load()
{
    L = luaL_newstate();
    luaL_openlibs(L);
    getGlobalNamespace(L)
        .beginNamespace("bot")
        .endNamespace();
    auto path = "modules/" + name + ".lua";
    if (luaL_dofile(L, path.c_str())) {
        auto str = Stack<string>::get(L, -1);
        printf("error loading module %s: %s\n", name.c_str(), str.c_str());
    }
}
