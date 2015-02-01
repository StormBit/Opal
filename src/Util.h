#ifndef BOT_UTIL_H
#define BOT_UTIL_H

#include <string>
#include <uv.h>

namespace std {

std::string to_string(struct addrinfo *addr);

}

namespace bot {

void print_loop(uv_loop_t *loop);

}

#endif
