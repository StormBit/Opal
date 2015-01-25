#ifndef BOT_LUA_MODULE_H
#define BOT_LUA_MODULE_H

#include <vector>
#include <unordered_map>
#include <string>
#include <lua.hpp>
#include <uv.h>

#include "LuaWrap.h"
#include "LuaRef.h"

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
    void reload();
    void register_file(const std::string &file);

    static int loader_wrapper(lua_State *L);
    static void event_cb(uv_fs_event_t *handle, const char *filename,
                         int events, int status);

    std::unordered_map<std::string, uv_fs_event_t> files;
    std::string name;
    lua_State *L = nullptr;
    int chunk = 0;
    LuaRef old_loader;
};

}

#endif
