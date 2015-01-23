#ifndef BOT_IRC_SERVER_H
#define BOT_IRC_SERVER_H

#include <string>
#include <vector>
#include <deque>
#include <uv.h>

#include "LuaWrap.h"
#include "IrcParser.h"
#include "EventBus.h"

namespace bot {

class IrcServer : public ObjectMixins<IrcServer> {
public:
    IrcServer(std::string address, std::string nickname, std::string user, std::string realname,
              std::vector<std::string> &&channels, EventBus &bus, char prefix = '+')
        : ObjectMixins<IrcServer>("IrcServer"), address(address), nickname(nickname), user(user),
        realname(realname), channels_to_join(channels), bus(bus), prefix(prefix) {}

    int start(uv_loop_t *loop, uv_tcp_t *tcp);
    void write(std::string &&str);
    void writef(const char *fmt, ...) __attribute((format(printf, 2, 3)));

    int __index(lua_State *L);

private:
    void begin_write();
    void handleMessage(const IrcMessage &msg);
    void handleCommand(const std::string &nick, const std::string &chan, const std::string &cmd, const std::string &args);
    static void alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf);
    static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void on_write(uv_write_t *req, int status);
    static int lua_write(lua_State *L);

    uv_tcp_t *tcp = nullptr;
    uv_loop_t *loop = nullptr;
    char buffer[65536] = "";
    std::deque<std::string> write_queue;
    unsigned write_num = 0;
    uv_write_t write_req;
    bool write_active = false;
    std::string address, nickname, user, realname;
    std::vector<std::string> channels_to_join;
    IrcParser parser;
    EventBus &bus;
    char prefix;
};

}

#endif
