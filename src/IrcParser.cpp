#include "IrcParser.h"

using namespace std;
using namespace bot;

void IrcParser::push(string &&str) {
    queue.emplace_back(str);
}

bool IrcParser::run(IrcMessage &out) {
    fp func = &IrcParser::read_prefix;
    while (!queue.empty() && func) {
        auto &str = queue.front();
        for (auto it = str.begin() + str_pos; it != str.end() && func; it++, str_pos++) {
            func = (this->*func)(*it);
        }
        if (func) {
            str_pos = 0;
            queue.pop_front();
        }
    }
    if (func) {
        return false;
    } else {
        out = cur;
        cur = IrcMessage();
        return true;
    }
}

IrcParser::fn IrcParser::read_prefix(char c) {
    switch (c) {
    case ':':
        return &IrcParser::read_nickname;
    case '\r':
    case '\n':
        return &IrcParser::read_prefix;
    default:
        return read_command(c);
    }
}

IrcParser::fn IrcParser::read_nickname(char c) {
    switch (c) {
    case '!':
        return &IrcParser::read_user;
    case '@':
        return &IrcParser::read_host;
    case ' ':
        return &IrcParser::read_command;
    default:
        cur.nickname.push_back(c);
        return &IrcParser::read_nickname;
    }
}

IrcParser::fn IrcParser::read_user(char c) {
    switch (c) {
    case '@':
        return &IrcParser::read_host;
    case ' ':
        return &IrcParser::read_command;
    default:
        cur.user.push_back(c);
        return &IrcParser::read_user;
    }
}

IrcParser::fn IrcParser::read_host(char c) {
    switch (c) {
    case ' ':
        return &IrcParser::read_command;
    default:
        cur.host.push_back(c);
        return &IrcParser::read_host;
    }
}

IrcParser::fn IrcParser::read_command(char c) {
    switch (c) {
    case ' ':
        return read_params(c);
    case '\r':
    case '\n':
        return nullptr;
    default:
        cur.command.push_back(c);
        return &IrcParser::read_command;
    }
}

IrcParser::fn IrcParser::read_params(char c) {
    switch (c) {
    case ' ':
        if (cur.params.empty() || !cur.params.back().empty()) {
            cur.params.push_back("");
        }
        return &IrcParser::read_params;
    case ':':
        cur.trailing = true;
        return &IrcParser::read_trailing;
    case '\r':
    case '\n':
    case '\0':
        return nullptr;
    default:
        cur.params.back().push_back(c);
        return &IrcParser::read_params;
    }
}

IrcParser::fn IrcParser::read_trailing(char c) {
    switch (c) {
    case '\n':
    case '\r':
    case '\0':
        return nullptr;
    default:
        cur.params.back().push_back(c);
        return &IrcParser::read_trailing;
    }
}
