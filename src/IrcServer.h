#ifndef BOT_IRC_SERVER_H
#define BOT_IRC_SERVER_H

#include <string>
#include <vector>
#include <deque>
#include <uv.h>

#include "LuaWrap.h"
#include "IrcParser.h"
#include "EventBus.h"
#include "Connection.h"

namespace bot {

static const char IrcServerName[] = "IrcServer";

class IrcServer : public ObjectMixins<IrcServer, IrcServerName> {
public:
    IrcServer(std::string address, std::string nickname, std::string user, std::string realname,
              std::vector<std::string> &&channels, EventBus &bus, char prefix = '+')
        : address(address), nickname(nickname), user(user), realname(realname),
          channels_to_join(channels), bus(bus), prefix(prefix) {}

    Connection::Error start(uv_loop_t *loop, const char *host, const char *service, bool use_tls);

    int __index(lua_State *L);

private:
    void handleMessage(const IrcMessage &msg);
    void handleCommand(const std::string &nick, const std::string &chan, const std::string &cmd, const std::string &args);
    static int lua_write(lua_State *L);

    Connection con;
    std::string address, nickname, user, realname;
    std::vector<std::string> channels_to_join;
    IrcParser parser;
    EventBus &bus;
    char prefix;
};

}

#endif
