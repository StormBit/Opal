#include "IrcServer.h"

#include <cstring>

using namespace std;
using namespace bot;

int IrcServer::start(uv_loop_t *loop, uv_tcp_t *tcp) {
    this->loop = loop;
    this->tcp = tcp;
    tcp->data = this;
    int res;
    if ((res = uv_read_start(reinterpret_cast<uv_stream_t*>(tcp), &IrcServer::alloc, &IrcServer::on_read))) {
        return res;
    }
    writef("NICK :%s\r\n", nickname.c_str());
    writef("USER %s 0 0 :%s\r\n", user.c_str(), realname.c_str());
    return 0;
}

void IrcServer::alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    (void)size;
    IrcServer &self = *reinterpret_cast<IrcServer*>(handle->data);
    buf->base = self.buffer;
    buf->len = sizeof(self.buffer);
}

void IrcServer::on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    IrcServer &self = *reinterpret_cast<IrcServer*>(stream->data);
    if (nread < 0) {
        printf("xxx uv read: %s\n", uv_strerror(nread));
        return;
    }
    self.parser.push(string(buf->base, nread));
    IrcMessage msg;
    while (self.parser.run(msg)) {
        self.handleMessage(msg);
    }
}

void IrcServer::begin_write() {
    if (!write_active && !write_queue.empty()) {
        vector<uv_buf_t> bufs;
        for (auto &str : write_queue) {
            bufs.emplace_back(uv_buf_t { const_cast<char*>(str.c_str()), str.size() });
        }
        write_num = write_queue.size();
        write_active = true;
        uv_write(&write_req, reinterpret_cast<uv_stream_t*>(tcp), bufs.data(), bufs.size(),
                 &IrcServer::on_write);
        write_req.data = this;
    }
}

void IrcServer::write(std::string &&str) {
    write_queue.emplace_back(str);
    begin_write();
}

void IrcServer::writef(const char *fmt, ...) {
    va_list ap1, ap2;
    va_start(ap1, fmt);
    ssize_t len = vsnprintf(NULL, 0, fmt, ap1);
    va_end(ap1);
    assert(len >= 0);
    char *buf = new char[len + 1];
    va_start(ap2, fmt);
    vsnprintf(buf, len+1, fmt, ap2);
    va_end(ap2);
    write(buf);
    delete[] buf;
}

void IrcServer::on_write(uv_write_t *req, int status) {
    IrcServer &self = *reinterpret_cast<IrcServer*>(req->data);
    if (status != 0) {
        printf("xxx %s\n", uv_strerror(status));
    } else {
        for (unsigned i = 0; i < self.write_num; i++) {
            self.write_queue.pop_front();
        }
        self.write_num = 0;
    }
    self.write_active = false;
    self.begin_write();
}

void IrcServer::handleMessage(const IrcMessage &msg) {
    printf("<<< %s\n", to_string(msg).c_str());
    if (msg.command == "001") {
        bus.fire("server-connect", EventBus::Value(static_cast<IObject&>(*this)));
        for (auto &c : channels_to_join) {
            writef("JOIN :%s\r\n", c.c_str());
        }
        return;
    }
    if (msg.command == "PING") {
        string v;
        for (unsigned i = 0; i < msg.params.size(); i++) {
            v += msg.params[i] + " ";
        }
        v.pop_back();
        writef("PONG :%s\r\n", v.c_str());
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

void IrcServer::handleCommand(const string &nick, const string &chan, const string &cmd, const string &args) {
    std::unique_ptr<EventBus::Value::Table> table(new EventBus::Value::Table {
            {"server", static_cast<IObject&>(*this)},
            {"nick", nick},
            {"channel", chan},
            {"command", cmd},
            {"args", args}
        });
    bus.fire("command", std::move(table));
    if (cmd == "echo") {
        writef("PRIVMSG %s :%s\r\n", chan.c_str(), args.c_str());
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
    self.write(str);
    return 0;
}
