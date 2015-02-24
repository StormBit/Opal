#include "IrcServer.h"

#include <cstring>

using namespace std;
using namespace bot;

Connection::Error IrcServer::start(uv_loop_t *loop, const char *host, const char *service, bool use_tls)
{
    con.connected_promise.then([=]() {
            con.writef("NICK :%s\r\n", nickname.c_str());
            con.writef("USER %s 0 0 :%s\r\n", user.c_str(), realname.c_str());
        });
    con.read_promise.then([=](uv_buf_t buf) {
            parser.push(string(buf.base, buf.len));
            IrcMessage msg;
            while (parser.run(msg)) {
                handleMessage(msg);
            }
        });
    con.error_promise.then([=](Connection::Error err) {
            printf("%s: %s\n", host, err.to_string().c_str());
        });
    return con.start(loop, host, service, use_tls);
}

void IrcServer::handleMessage(const IrcMessage &msg)
{
    printf("<<< %s\n", to_string(msg).c_str());
    if (msg.command == "001") {
        bus.fire("server-connect", Value(static_cast<IObject&>(*this)));
        for (auto &c : channels_to_join) {
            con.writef("JOIN :%s\r\n", c.c_str());
        }
        return;
    }
    if (msg.command == "PING") {
        string v;
        for (unsigned i = 0; i < msg.params.size(); i++) {
            v += msg.params[i] + " ";
        }
        v.pop_back();
        con.writef("PONG :%s\r\n", v.c_str());
        return;
    }
    if (msg.command == "JOIN") {
        unique_ptr<Value::Table> table(new Value::Table {
                {"server", static_cast<IObject&>(*this)},
                {"nick", msg.nickname},
                {"channel", msg.params[0]}
            });
        bus.fire("join", move(table));
    }
    if (msg.command == "PRIVMSG") {
        if (msg.params.size() < 2) {
            return;
        }
        unique_ptr<Value::Table> table(new Value::Table {
                {"server", static_cast<IObject&>(*this)},
                {"nick", msg.nickname},
                {"channel", msg.params[0]},
                {"message", msg.params[1]}
            });
        bus.fire("message", move(table));
        if (nickname == msg.params[0]) {
            handleCommand(msg.nickname, msg.nickname, msg.params[0], msg.params[1]);
            return;
        }
        if (msg.params[1][0] == prefix) {
            auto &args = msg.params[1];
            size_t split = args.find(" ");
            string tail;
            if (split != string::npos) {
                tail = args.substr(split+1, string::npos);
            }
            string head = args.substr(1, split == string::npos? string::npos : split-1);
            handleCommand(msg.nickname, msg.params[0], head, tail);
            return;
        }
    }
}

void IrcServer::handleCommand(const string &nick, const string &chan, const string &cmd, const string &args)
{
    std::unique_ptr<Value::Table> table(new Value::Table {
            {"server", static_cast<IObject&>(*this)},
            {"nick", nick},
            {"channel", chan},
            {"command", cmd},
            {"args", args}
        });
    bus.fire("command", std::move(table));
    if (cmd == "echo") {
        con.writef("PRIVMSG %s :%s\r\n", chan.c_str(), args.c_str());
        return;
    }
}

int IrcServer::__index(lua_State *L)
{
    const char *key = luaL_checkstring(L, 2);

    if (!strcmp(key, "address")) {
        lua_pushlstring(L, address.c_str(), address.size());
        return 1;
    }
    if (!strcmp(key, "nickname")) {
        lua_pushlstring(L, nickname.c_str(), address.size());
        return 1;
    }
    if (!strcmp(key, "user")) {
        lua_pushlstring(L, user.c_str(), user.size());
        return 1;
    }
    if (!strcmp(key, "realname")) {
        lua_pushlstring(L, realname.c_str(), realname.size());
        return 1;
    }
    if (!strcmp(key, "prefix")) {
        lua_pushfstring(L, "%c", prefix);
        return 1;
    }
    if (!strcmp(key, "write")) {
        lua_pushcfunction(L, &IrcServer::lua_write);
        return 1;
    }
    return 0;
}

int IrcServer::lua_write(lua_State *L)
{
    IrcServer &self = unwrap(L, 1);
    size_t len = 0;
    const char *str = luaL_checklstring(L, 2, &len);
    if (len > strlen(str)) {
        luaL_error(L, "IRC messages cannot contain embedded zeros");
    }
    self.con.write(str);
    return 0;
}
