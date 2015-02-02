#ifndef BOT_LUA_MODULE_H
#define BOT_LUA_MODULE_H

#include <vector>
#include <unordered_map>
#include <string>
#include <lua.hpp>
#include <uv.h>

#include "LuaWrap.h"
#include "LuaRef.h"
#include "EventBus.h"
#include "Sqlite.h"

namespace bot {

const char LuaModuleName[] = "LuaModule";

class LuaModule : public ObjectMixins<LuaModule, LuaModuleName> {
public:
    LuaModule(std::string &&name) : name(name) {}

    void loadrun(uv_loop_t *loop);
    void load(uv_loop_t *loop);
    void run();
    lua_State *getL() {
        return L;
    }

    static LuaModule &getModule(lua_State *L);

    uv_loop_t *loop = nullptr;
    LuaBus bus;

private:
    void reload();
    void openlibs();
    void register_file(const std::string &file);

    static int loader_wrapper(lua_State *L);
    static int lua_now(lua_State *L);
    static void event_cb(uv_fs_event_t *handle, const char *filename,
                         int events, int status);

    std::unordered_map<std::string, uv_fs_event_t> files;
    std::string name;
    lua_State *L = nullptr;
    LuaRef chunk;
    Database db;
};

}

#endif
