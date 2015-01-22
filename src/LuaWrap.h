#ifndef BOT_LUA_WRAP_H
#define BOT_LUA_WRAP_H

#include <utility>
#include <typeinfo>
#include <memory>
#include <lua.hpp>

namespace bot {

class IObject {
public:
    virtual ~IObject();
    virtual const char *tname() const = 0;
    virtual size_t hash() const = 0;
    virtual void meta(lua_State *L) const = 0;
};

template<class T>
class ObjectMixins : public IObject {
    
};

template<class T>
class LuaWrap {
public:
    static LuaWrap &create(lua_State *L, T &value)
    {
        return *new (lua_newuserdata(L, sizeof(LuaWrap))) LuaWrap(value);
    }
    static LuaWrap &create(lua_State *L, T &&value)
    {
        return *new (lua_newuserdata(L, sizeof(LuaWrap))) LuaWrap(std::move(value));
    }
    static LuaWrap &create(lua_State *L, IObject *object)
    {
        return *new (lua_newuserdata(L, sizeof(LuaWrap))) LuaWrap(object);
    }
    static LuaWrap &create(lua_State *L, std::unique_ptr<IObject> &&object)
    {
        return *new (lua_newuserdata(L, sizeof(LuaWrap))) LuaWrap(std::move(object));
    }
    template<typename ...Args>
    static LuaWrap &create(lua_State *L, Args &&...args)
    {
        return *new (lua_newuserdata(L, sizeof(LuaWrap))) LuaWrap(std::forward(args)...);
    }

    T &get() {
        assert(typeid(T).hash_code() == hash);
        return *value;
    }

    static void add_gc(lua_State *L, int idx = -3) {
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, &LuaWrap::__gc);
        lua_settable(L, idx);
    }

private:
    LuaWrap(T &value) : value(&value), hash(typeid(T).hash_code()), owned(false) {}
    LuaWrap(T &&value) : value(new T(std::move(value))), hash(typeid(T).hash_code()), owned(true) {}
    LuaWrap(IObject *object) : value(object), hash(object->hash()), owned(false) {}
    LuaWrap(std::unique_ptr<IObject> &&object)
        : value(object.release()), hash(value->hash()), owned(true) {}
    template<typename ...Args>
    LuaWrap(Args &&...args)
        : value(new T(std::forward(args)...)), hash(typeid(T).hash_code()), owned(true) {}
    LuaWrap(LuaWrap&) = delete;
    ~LuaWrap() {
        if (owned) {
            value->~T();
        }
    }

    static int __gc(lua_State *L)
    {
        LuaWrap *wrap = reinterpret_cast<LuaWrap*>(lua_touserdata(L, 1));
        assert(typeid(T).hash_code() == wrap->hash);
        wrap->~LuaWrap();
        return 0;
    }

    T *value;
    size_t hash;
    bool owned;
};

}

#endif
