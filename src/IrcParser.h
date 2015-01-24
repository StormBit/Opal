#ifndef BOT_IRC_PARSER_H
#define BOT_IRC_PARSER_H

#include <string>
#include <deque>
#include <vector>
#include <uv.h>

namespace bot {

struct IrcMessage {
    std::string nickname, user, host, command;
    std::vector<std::string> params;
    bool trailing = false;
};

class IrcParser {
public:
    void push(std::string &&str);
    bool run(IrcMessage &out);

private:
    struct fn;
    typedef fn (IrcParser::*fp)(char c);

    struct fn {
        fn(fp f) : f(f) {}
        operator fp() { return f; }
        fp f;
    };

    fn read_prefix(char);
    fn read_nickname(char);
    fn read_user(char);
    fn read_host(char);
    fn read_command(char);
    fn read_params(char);
    fn read_trailing(char);

    IrcMessage cur;
    std::deque<std::string> queue;
    unsigned str_pos = 0;
};

}

namespace std {

inline
std::string to_string(const bot::IrcMessage &msg)
{
    string acc;
    if (!msg.nickname.empty()) {
        acc += ":" + msg.nickname;
        if (!msg.user.empty()) {
            acc += "!" + msg.user;
        }
        if (!msg.host.empty()) {
            acc += "@" + msg.host;
        }
        acc += " ";
    }
    acc += msg.command;
    if (!msg.params.empty()) {
        for (unsigned i = 0; i < msg.params.size() - 1; i++) {
            acc += " " + msg.params[i];
        }
        if (msg.trailing) {
            acc += " :" + msg.params.back();
        } else {
            acc += " " + msg.params.back();
        }
    }
    return acc;
}

}

#endif
