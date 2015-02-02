#ifndef BOT_SQLITE_H
#define BOT_SQLITE_H

#include <sqlite3.h>
#include <cstring>

#include "LuaWrap.h"

namespace bot {

const char DatabaseName[] = "Database";
const char StatementName[] = "Database.Statement";

class Database : public ObjectMixins<Database, DatabaseName> {
public:
    class Statement : public ObjectMixins<Statement, StatementName> {
    public:
        Statement() = default;
        Statement(Statement &&);
        ~Statement();

        int finalize();
        int step();
        int reset();
        int bind(int index, const void *data, uint64_t len);
        int bind(int index, double value);
        int bind(int index, int value);
        int bind(int index, int64_t value);
        int bind(int index);
        int bind(int index, const char *value, uint64_t len);
        int column_type(int col);
        const void *column_blob(int col, uint64_t *len);
        double      column_double(int col);
        int         column_int(int col);
        int64_t     column_int64(int col);
        const char *column_text(int col);
        const char *column_name(int col);
        unsigned    column_count();

        int __index(lua_State *L);

    private:
        int lua_column(lua_State *L, int idx);
        static int lua_finalize(lua_State *L);
        static int lua_step(lua_State *L);
        static int lua_reset(lua_State *L);
        static int lua_bind(lua_State *L);
        static int lua_get(lua_State *L);

        Statement(sqlite3_stmt *stmt) : stmt(stmt) {}

        sqlite3_stmt *stmt = nullptr;

        friend Database;
    };

    Database() = default;
    Database(Database &&);
    ~Database();

    int open(const char *filename);
    int close();
    std::pair<int, Statement> prepare(const char *sql, size_t len);

    int __index(lua_State *L);

private:
    static int lua_open(lua_State *L);
    static int lua_close(lua_State *L);
    static int lua_prepare(lua_State *L);
    static int lua_table(lua_State *L);

    sqlite3 *db = nullptr;
};

}

#endif
