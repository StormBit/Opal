#ifndef BOT_IRC_SERVER_H
#define BOT_IRC_SERVER_H

#include <string>
#include <vector>
#include <deque>
#include <uv.h>

#include "LuaWrap.h"
#include "IrcParser.h"
#include "EventBus.h"
#include "TcpConnection.h"
#include "DnsResolver.h"

namespace bot {

static const char IrcServerName[] = "IrcServer";

class IrcServer : public ObjectMixins<IrcServer, IrcServerName> {
public:
    IrcServer(std::string address, std::string nickname, std::string user, std::string realname,
              std::vector<std::string> &&channels, EventBus &bus, char prefix = '+')
        : tcp(*this), dns(tcp), address(address), nickname(nickname), user(user), realname(realname),
          channels_to_join(channels), bus(bus), prefix(prefix) {}

    int start(const char *host, const char *service, uv_loop_t *loop);
    int start(uv_loop_t *loop, uv_tcp_t *tcp);

    void onRead(ssize_t nread, const uv_buf_t *buf);
    void onError(int status);
    int __index(lua_State *L);

private:
    void handleMessage(const IrcMessage &msg);
    void handleCommand(const std::string &nick, const std::string &chan, const std::string &cmd, const std::string &args);
    static int lua_write(lua_State *L);

    TcpConnection<IrcServer> tcp;
    DnsResolver<TcpConnection<IrcServer>> dns;
    std::string address, nickname, user, realname;
    std::vector<std::string> channels_to_join;
    IrcParser parser;
    EventBus &bus;
    char prefix;
};

}

#endif
