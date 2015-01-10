#ifndef BOT_IRC_PARSER_H
#define BOT_IRC_PARSER_H

#include <string>
#include <deque>
#include <vector>

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

#endif
