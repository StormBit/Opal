#ifndef BOT_EVENT_BUS_H
#define BOT_EVENT_BUS_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <lua.hpp>
#include <assert.h>

#include "LuaWrap.h"
#include "LuaRef.h"
#include "Value.h"

namespace bot {

static const char LuaBusName[] = "EventBus";

class LuaBus : public ObjectMixins<LuaBus, LuaBusName> {
public:
    void openlib(lua_State *L);
    void print();
    void fire(const std::string &name, const Value &val);
    void clear();

    int __index(lua_State *L);

private:
    static int lua_hook(lua_State *L);
    static int lua_fire(lua_State *L);

    std::unordered_map<std::string, std::vector<LuaRef>> lua_handlers;
};

class EventBus {
public:
    void fire(const std::string &name, const Value &val);
    void hook(std::string &&name, std::function<void (const Value &val)> &&fn);
    void print();
    void addBus(LuaBus &bus);

private:
    std::unordered_map<std::string, std::vector<std::function<void (const Value &val)>>> handlers;
    std::vector<LuaBus*> lua_buses;
};

}

#endif
