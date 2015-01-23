#include "LuaWrap.h"

using namespace bot;

IObject::~IObject() {}

LuaWrap<IObject> &bot::wrap_object(IObject *ptr, lua_State *L) {
    auto &w = LuaWrap<IObject>::create(L, ptr);
    if (luaL_newmetatable(L, ptr->tname())) {
        ptr->meta(L);
    }
    lua_setmetatable(L, -2);
    return w;
}
