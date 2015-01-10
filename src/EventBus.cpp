#include "EventBus.h"

using namespace bot;
using namespace std;
using namespace luabridge;

void EventBus::fire(const string &name, const Value &val)
{
    auto it1 = handlers.find(name);
    if (it1 != handlers.end()) {
        for (auto &h : it1->second) {
            h(val);
        }
    }
    auto it2 = lua_handlers.find(name);
    if (it2 != lua_handlers.end()) {
        for (auto &h : it2->second) {
            h(val);
        }
    }
}

void EventBus::hook(string &&name, function<void (const Value &val)> &&fn)
{
    auto res = handlers.emplace(piecewise_construct,
                                forward_as_tuple(name),
                                forward_as_tuple());
    res.first->second.emplace_back(fn);
}

void EventBus::openlib(lua_State *L)
{
    getGlobalNamespace(L)
        .beginNamespace("bot")
          .beginClass<EventBus>("event")
            .addFunction("fire", &EventBus::fire)
            .addFunction("hook", &EventBus::hookLua)
          .endClass()
          .beginClass<Value>("value")
            .addProperty("type", &EventBus::Value::strtype)
            .addProperty("number", &EventBus::Value::number)
            .addProperty("string", &EventBus::Value::string)
            .addProperty("bool", &EventBus::Value::boolean)
            .addProperty("table", &EventBus::Value::table)
            .addFunction("get", &EventBus::Value::get)
          .endClass()
        .endNamespace();
}

void EventBus::hookLua(const string &name, LuaRef fn)
{
    auto res = lua_handlers.emplace(piecewise_construct,
                                    forward_as_tuple(name),
                                    forward_as_tuple());
    res.first->second.emplace_back(fn);
}
