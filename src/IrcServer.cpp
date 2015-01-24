#include "IrcServer.h"

#include <cstring>

using namespace std;
using namespace bot;

int IrcServer::start(const char *host, const char *service, uv_loop_t *loop)
{
    return dns.start(host, service, loop);
}

int IrcServer::start(uv_loop_t *loop, uv_tcp_t *tcp_)
{
    (void)loop, (void)tcp_;
    tcp.writef("NICK :%s\r\n", nickname.c_str());
    tcp.writef("USER %s 0 0 :%s\r\n", user.c_str(), realname.c_str());
    return 0;
}

void IrcServer::onRead(ssize_t nread, const uv_buf_t *buf)
{
    if (nread < 0) {
        printf("xxx uv read: %s\n", uv_strerror(nread));
        return;
    }
    parser.push(string(buf->base, nread));
    IrcMessage msg;
    while (parser.run(msg)) {
        handleMessage(msg);
    }
}

void IrcServer::onError(int status)
{
    printf("xxx: %s\n", uv_strerror(status));
}

void IrcServer::handleMessage(const IrcMessage &msg)
{
    printf("<<< %s\n", to_string(msg).c_str());
    if (msg.command == "001") {
        bus.fire("server-connect", EventBus::Value(static_cast<IObject&>(*this)));
        for (auto &c : channels_to_join) {
            tcp.writef("JOIN :%s\r\n", c.c_str());
        }
        return;
    }
    if (msg.command == "PING") {
        string v;
        for (unsigned i = 0; i < msg.params.size(); i++) {
            v += msg.params[i] + " ";
        }
        v.pop_back();
        tcp.writef("PONG :%s\r\n", v.c_str());
        return;
    }
    if (msg.command == "PRIVMSG") {
        if (msg.params.size() < 2) {
            return;
        }
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
    std::unique_ptr<EventBus::Value::Table> table(new EventBus::Value::Table {
            {"server", static_cast<IObject&>(*this)},
            {"nick", nick},
            {"channel", chan},
            {"command", cmd},
            {"args", args}
        });
    bus.fire("command", std::move(table));
    if (cmd == "echo") {
        tcp.writef("PRIVMSG %s :%s\r\n", chan.c_str(), args.c_str());
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
    self.tcp.write(str);
    return 0;
}
