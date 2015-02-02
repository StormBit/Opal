#include "Sqlite.h"

using namespace bot;
using namespace std;

Database::Statement::Statement(Statement &&other)
{
    stmt = other.stmt;
    other.stmt = nullptr;
}

Database::Statement::~Statement()
{
    finalize();
}

int Database::Statement::finalize()
{
    if (stmt) {
        int res = sqlite3_finalize(stmt);
        stmt = nullptr;
        return res;
    }
    return 0;
}

int Database::Statement::step()
{
    return sqlite3_step(stmt);
}

int Database::Statement::reset()
{
    return sqlite3_reset(stmt);
}

int Database::Statement::bind(int index, const void *data, uint64_t len) {
    return sqlite3_bind_blob(stmt, index, data, len, SQLITE_TRANSIENT);
}
int Database::Statement::bind(int index, double value) {
    return sqlite3_bind_double(stmt, index, value);
}
int Database::Statement::bind(int index, int value) {
    return sqlite3_bind_int(stmt, index, value);
}
int Database::Statement::bind(int index, int64_t value) {
    return sqlite3_bind_int64(stmt, index, value);
}
int Database::Statement::bind(int index) {
    return sqlite3_bind_null(stmt, index);
}
int Database::Statement::bind(int index, const char *value, uint64_t len) {
    return sqlite3_bind_text64(stmt, index, value, len, SQLITE_TRANSIENT, SQLITE_UTF8);
}
int Database::Statement::column_type(int col) {
    return sqlite3_column_type(stmt, col);
}
const void *Database::Statement::column_blob(int col, uint64_t *len) {
    if (len) {
        *len = (uint64_t)sqlite3_column_bytes(stmt, col);
    }
    return sqlite3_column_blob(stmt, col);
}
double Database::Statement::column_double(int col) {
    return sqlite3_column_double(stmt, col);
}
int Database::Statement::column_int(int col) {
    return sqlite3_column_int(stmt, col);
}
int64_t Database::Statement::column_int64(int col) {
    return sqlite3_column_int64(stmt, col);
}
const char *Database::Statement::column_text(int col) {
    return reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
}
const char *Database::Statement::column_name(int col) {
    return sqlite3_column_name(stmt, col);
}
unsigned Database::Statement::column_count() {
    return sqlite3_column_count(stmt);
}


int Database::Statement::__index(lua_State *L)
{
    if (lua_isnumber(L, 2)) {
        int idx = lua_tointeger(L, 2);
        return lua_column(L, idx);
    }
    const char *key = luaL_checkstring(L, 2);

    if (!strcmp(key, "finalize")) {
        lua_pushcfunction(L, &Database::Statement::lua_finalize);
        return 1;
    }
    if (!strcmp(key, "step")) {
        lua_pushcfunction(L, &Database::Statement::lua_step);
        return 1;
    }
    if (!strcmp(key, "reset")) {
        lua_pushcfunction(L, &Database::Statement::lua_reset);
        return 1;
    }
    if (!strcmp(key, "bind")) {
        lua_pushcfunction(L, &Database::Statement::lua_bind);
        return 1;
    }
    if (!strcmp(key, "get")) {
        lua_pushcfunction(L, &Database::Statement::lua_get);
        return 1;
    }
    return 0;
}

int Database::Statement::lua_column(lua_State *L, int idx)
{
    auto &self = unwrap(L, 1);
    switch (self.column_type(idx)) {
    case SQLITE_INTEGER:
        lua_pushinteger(L, self.column_int(idx));
        return 1;
    case SQLITE_FLOAT:
        lua_pushnumber(L, self.column_double(idx));
        return 1;
    case SQLITE_BLOB:
        {
            size_t len;
            const char *blob = reinterpret_cast<const char*>(self.column_blob(idx, &len));
            lua_pushlstring(L, blob, len);
            return 1;
        }
    case SQLITE_NULL:
        lua_pushnil(L);
        return 1;
    case SQLITE3_TEXT:
        lua_pushstring(L, self.column_text(idx));
        return 1;
    default:
        luaL_error(L, "Unknown SQLite type");
        return 0;
    }
}

int Database::Statement::lua_finalize(lua_State *L)
{
    auto &self = unwrap(L, 1);
    self.finalize();
    return 0;
}

int Database::Statement::lua_step(lua_State *L)
{
    auto &self = unwrap(L, 1);
    self.step();
    return 0;
}

int Database::Statement::lua_reset(lua_State *L)
{
    auto &self = unwrap(L, 1);
    self.reset();
    return 0;
}

int Database::Statement::lua_bind(lua_State *L)
{
    auto &self = unwrap(L, 1);
    int idx = luaL_checkint(L, 2);
    switch (lua_type(L, 3)) {
    case LUA_TNONE:
    case LUA_TNIL:
        self.bind(idx);
        break;
    case LUA_TNUMBER:
        self.bind(idx, lua_tonumber(L, 3));
        break;
    case LUA_TBOOLEAN:
        self.bind(idx, lua_toboolean(L, 3));
        break;
    case LUA_TSTRING:
        {
            size_t len;
            const char *str = lua_tolstring(L, 3, &len);
            self.bind(idx, str, len);
            break;
        }
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
        luaL_error(L, "Type %s cannot be bound to an SQL Statement", lua_typename(L, lua_type(L, 3)));
        break;
    }
    return 0;
}

int Database::Statement::lua_get(lua_State *L)
{
    auto &self = unwrap(L, 1);
    lua_newtable(L);
    int table = lua_gettop(L);
    unsigned row_num = 0;
    int res;
    if ((res = self.reset()) != SQLITE_OK) {
        printf("invalid start state: %s\n", sqlite3_errstr(res));
    }
    while ((res = self.step())) {
        switch (res) {
        case SQLITE_ROW:
            {
                unsigned count = self.column_count();
                lua_pushinteger(L, row_num);
                lua_newtable(L);
                int row = lua_gettop(L);
                for (unsigned i = 0; i < count; i++) {
                    lua_pushstring(L, self.column_name(i));
                    if (self.lua_column(L, i) > 0) {
                        lua_settable(L, row);
                    } else {
                        lua_pop(L, 1);
                    }
                    assert(row == lua_gettop(L));
                }
                lua_settable(L, table);
                row_num++;
                break;
            }
        case SQLITE_DONE:
            self.reset();
            assert(lua_gettop(L) == table);
            return row_num > 0;
        default:
            self.reset();
            luaL_error(L, "step: %s", sqlite3_errstr(res));
            return 0;
        }
    }

    return 0;
}

Database::Database(Database &&other)
{
    db = other.db;
    other.db = nullptr;
}

Database::~Database()
{
    close();
}

int Database::open(const char *filename)
{
    if (!db) {
        return sqlite3_open_v2(filename, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    }
    return 0;
}

int Database::close()
{
    if (db) {
        int res = sqlite3_close(db);
        db = nullptr;
        return res;
    }
    return 0;
}

pair<int, Database::Statement> Database::prepare(const char *sql, size_t len)
{
    sqlite3_stmt *stmt;
    int res = sqlite3_prepare_v2(db, sql, len, &stmt, NULL);
    Statement out;
    if (!res) {
        new (&out) Statement(stmt);
    }
    return make_pair(res, move(out));
}

int Database::__index(lua_State *L)
{
    const char *key = luaL_checkstring(L, 2);

    if (!strcmp(key, "open")) {
        lua_pushcfunction(L, &Database::lua_open);
        return 1;
    }
    if (!strcmp(key, "close")) {
        lua_pushcfunction(L, &Database::lua_close);
        return 1;
    }
    if (!strcmp(key, "prepare")) {
        lua_pushcfunction(L, &Database::lua_prepare);
        return 1;
    }
    if (!strcmp(key, "table")) {
        lua_pushcfunction(L, &Database::lua_table);
        return 1;
    }
    return 0;
}

int Database::lua_open(lua_State *L)
{
    auto &self = unwrap(L, 1);
    const char *filename = luaL_checkstring(L, 2);
    self.open(filename);
    return 0;
}

int Database::lua_close(lua_State *L)
{
    unwrap(L, 1).close();
    return 0;
}

int Database::lua_prepare(lua_State *L)
{
    auto &self = unwrap(L, 1);
    size_t len;
    const char *sql = luaL_checklstring(L, 2, &len);
    auto p = self.prepare(sql, len);
    if (p.first) {
        luaL_error(L, "prepare: %s", sqlite3_errstr(p.first));
        return 0;
    }
    Database::Statement::wrap_move(move(p.second), L);
    return 1;
}

int Database::lua_table(lua_State *L)
{
    auto &self = unwrap(L, 1);
    lua_pushstring(L, "name");
    lua_gettable(L, 2);
    const char *name = luaL_checkstring(L, -1);
    string query = "CREATE TABLE IF NOT EXISTS ";
    query += name;
    query += "(";
    size_t len = lua_rawlen(L, 2);
    for (unsigned i = 1; i <= len; i++) {
        lua_pushinteger(L, i);
        lua_gettable(L, 2);
        const char *t = luaL_checkstring(L, -1);
        query += t;
        if (i < len) {
            query += ", ";
        }
    }
    query += ");";
    auto p = self.prepare(query.c_str(), query.size());
    if (p.first) {
        luaL_error(L, "database.prepare in database.table: %s; query: %s", sqlite3_errstr(p.first), query.c_str());
        return 0;
    }
    int res = p.second.step();
    if (res != SQLITE_DONE) {
        luaL_error(L, "database.step in database.table: %s; query: %s", sqlite3_errstr(res), query.c_str());
        return 0;
    }
    return 0;
}
