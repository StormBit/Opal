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
std::string to_string(struct addrinfo *addr)
{
    char buf[4096];
    void *a = NULL;
    unsigned short port = 0;
    struct sockaddr_in *sa;
    struct sockaddr_in6 *sa6;
    switch (addr->ai_family) {
    case AF_INET:
        sa = reinterpret_cast<struct sockaddr_in*>(addr->ai_addr);
        a = &sa->sin_addr;
        port = sa->sin_port;
        break;
    case AF_INET6:
        sa6 = reinterpret_cast<struct sockaddr_in6*>(addr->ai_addr);
        a = &sa6->sin6_addr;
        port = sa6->sin6_port;
        break;
    default:
        break;
    }
    return string(inet_ntop(addr->ai_family, a, buf, 4096)) + ":" + to_string(ntohs(port));
}

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
