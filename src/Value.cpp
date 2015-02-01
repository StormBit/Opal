#include "Value.h"

using namespace bot;
using namespace std;

void Value::wrap(lua_State *L) const
{
    switch (type()) {
    case Type::Nil:
        lua_pushnil(L);
        break;
    case Type::Number:
        lua_pushnumber(L, number());
        break;
    case Type::String:
        lua_pushlstring(L, string().c_str(), string().size());
        break;
    case Type::Bool:
        lua_pushboolean(L, boolean());
        break;
    case Type::Table:
        {
            const Value::Table &t = table();
            lua_createtable(L, 0, t.size());
            int idx = lua_gettop(L);
            for (auto &kv : t) {
                kv.first.wrap(L);
                kv.second.wrap(L);
                lua_settable(L, idx);
            }
            break;
        }
    case Type::OwnedObject:
        assert(!"Cannot copy owned object");
    case Type::RefObject:
        wrap_object(ref_object_, L);
        break;
    }
}

Value Value::unwrap(lua_State *L)
{
    int vtype = lua_type(L, -1);
    Value value;
    switch (vtype) {
    case LUA_TNONE:
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        value = Value(lua_tonumber(L, -1));
        break;
    case LUA_TBOOLEAN:
        value = Value((bool)lua_toboolean(L, -1));
        break;
    case LUA_TSTRING:
        value = Value(::string(lua_tostring(L, -1)));
        break;
    case LUA_TTABLE:
        luaL_error(L, "NYI");
        break;
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
    default:
        luaL_error(L, "Unsupported type %s", lua_typename(L, -1));
        break;
    }
    return value;
}

string Value::to_string(size_t indent) const
{
    switch (type()) {
    case Type::Nil:
        return "nil";
    case Type::Number:
        return ::to_string(number());
    case Type::String:
        return "\"" + string() + "\"";
    case Type::Bool:
        return boolean()? "true" : "false";
    case Type::Table:
        {
            ::string str;
            ::string outer(indent * 2, ' ');
            ::string inner((indent+1) * 2, ' ');
            str.append("{");
            for (auto &kv : table()) {
                str.append(inner);
                str.append(kv.first.to_string(indent+1));
                str.append(": ");
                str.append(kv.second.to_string(indent+1));
                str.append("\n");
            }
            str.append(outer);
            str.append("}");
            return str;
        }
    case Type::OwnedObject:
        {
            char buf[64];
            sprintf(buf, "owned %s @%p", owned_object_->tname(), (void*)&*owned_object_);
            return buf;
        }
    case Type::RefObject:
        {
            char buf[64];
            sprintf(buf, "ref %s @%p", ref_object_->tname(), (void*)ref_object_);
            return buf;
        }
    }
}

void Value::print() const
{
    printf("%s\n", to_string().c_str());
}
