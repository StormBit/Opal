#ifndef BOT_UTIL_H
#define BOT_UTIL_H

#include <string>

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

}

#endif
