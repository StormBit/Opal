#include "LuaModule.h"

#include <cstring>
#include <unistd.h>

#include "HttpRequest.h"

using namespace std;
using namespace bot;

void LuaModule::loadrun(uv_loop_t *loop)
{
    load(loop);
    run();
}

void LuaModule::load(uv_loop_t *loop)
{
    this->loop = loop;
    L = luaL_newstate();
    lua_pushstring(L, "LuaModuleInstance");
    wrap_object(this, L);
    lua_rawset(L, LUA_REGISTRYINDEX);
    openlibs();

    reload();
}

void LuaModule::reload()
{
    bus.clear();

    {
        int top = lua_gettop(L);
        lua_getglobal(L, "package");
        lua_pushstring(L, "loaded");
        lua_newtable(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    auto path = "modules/" + name + ".lua";
    register_file(path);
    if (luaL_loadfile(L, path.c_str())) {
        const char *str = lua_tostring(L, -1);
        printf("error loading module %s: %s\n", name.c_str(), str);
    } else {
        chunk = LuaRef::create(L);
    }
}

void LuaModule::run()
{
    if (chunk.empty()) {
        printf("empty chunk\n");
        return;
    }
    chunk.push();
    if (lua_pcall(L, 0, 0, 0)) {
        const char *str = lua_tostring(L, -1);
        printf("error running module %s: %s\n", name.c_str(), str);
    }
}

void LuaModule::openlibs()
{
    luaL_openlibs(L);

    {
        int top = lua_gettop(L);
        lua_getglobal(L, "package");
        lua_pushstring(L, "loaders");
        lua_gettable(L, -2);
        int loaders = lua_gettop(L);
        lua_rawgeti(L, loaders, 2);
        wrap_object(this, L);
        lua_pushcclosure(L, &LuaModule::loader_wrapper, 2);
        lua_rawseti(L, loaders, 2);
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
    }

    bus.openlib(L);
    HttpRequest::openlib(L);

    {
        int res = mkdir("data/", 0755);
        if (res != 0 && errno != EEXIST) {
            perror("mkdir data/");
            abort();
        }
        string path = "data/" + name + ".sqlite";
        res = db.open(path.c_str());
        if (res) {
            printf("db.open: %s\n", sqlite3_errstr(res));
        }
        wrap_object(&db, L);
        lua_setglobal(L, "database");
    }

    lua_pushlightuserdata(L, loop);
    lua_pushcclosure(L, &LuaModule::lua_now, 1);
    lua_setglobal(L, "now");
}

LuaModule &LuaModule::getModule(lua_State *L)
{
    lua_pushstring(L, "LuaModuleInstance");
    lua_rawget(L, LUA_REGISTRYINDEX);
    return reinterpret_cast<LuaWrap<LuaModule>*>(luaL_checkudata(L, -1, LuaModuleName))->get();
}

void LuaModule::register_file(const std::string &file)
{
    auto res = files.emplace(file, uv_fs_event_t());
    if (res.second) {
        printf("Watch %s\n", file.c_str());
        uv_fs_event_init(loop, &res.first->second);
        uv_fs_event_start(&res.first->second, &LuaModule::event_cb, file.c_str(), 0);
        res.first->second.data = this;
    }
}

int LuaModule::loader_wrapper(lua_State *L)
{
    auto &self = LuaModule::unwrap(L, lua_upvalueindex(2));
    const char *name = luaL_checkstring(L, 1);

    int top = lua_gettop(L);
    lua_getglobal(L, "package");
    lua_pushstring(L, "path");
    lua_rawget(L, -2);
    size_t len;
    const char *path = luaL_checklstring(L, -1, &len);
    const char *tok_start = path, *tok_end = NULL, *question = NULL;
    for (unsigned i = 0; i < len+1; i++) {
        switch (path[i]) {
        case 0:
        case ';':
            {
                tok_end = path + i;
                string npath;
                if (tok_start == tok_end) {
                    luaL_error(L, "Default path NYI");
                }
                if (!question) {
                    luaL_error(L, "Invalid PATH: No ? in pattern");
                }
                npath += string(tok_start, question - tok_start);
                npath += name;
                npath += string(question + 1, tok_end - question - 1);
                if (!access(npath.c_str(), F_OK)) {
                    self.register_file(npath);
                }
                tok_start = path + i + 1;
                question = NULL;
                break;
            }
        case '?':
            question = path + i;
            break;
        default:
            continue;
        }
    }
    lua_pop(L, 2);
    assert(top == lua_gettop(L));
    lua_pushvalue(L, lua_upvalueindex(1));
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    lua_call(L, 1, LUA_MULTRET);
    return lua_gettop(L) - top;
}

int LuaModule::lua_now(lua_State *L)
{
    uv_loop_t *loop = reinterpret_cast<uv_loop_t*>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushinteger(L, uv_now(loop));
    return 1;
}

void LuaModule::event_cb(uv_fs_event_t *handle, const char *filename,
                         int events, int status)
{
    (void)events, (void)status;
    auto &self = *reinterpret_cast<LuaModule*>(handle->data);
    printf("%s changed, reloading %s\n", filename, self.name.c_str());
    self.reload();
    self.run();
}
