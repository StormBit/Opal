#ifndef BOT_DNS_RESOLVER_H
#define BOT_DNS_RESOLVER_H

#include <cstdio>
#include <uv.h>

#include "Util.h"

namespace bot {

template<class T>
struct DnsResolver {
    uv_getaddrinfo_t req;
    const char *node = nullptr, *service = nullptr;
    uv_loop_t *loop = nullptr;
    T &ready;

    DnsResolver(T &ready)
        : ready(ready) {}

    int start(const char *node, const char *service, uv_loop_t *loop) {
        this->node = node;
        this->service = service;
        this->loop = loop;
        return uv_getaddrinfo(loop, &req, &DnsResolver::callback, node, service, NULL);
    }

    static void callback(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
        (void)status;
        DnsResolver &self = *reinterpret_cast<DnsResolver*>(req);
        if (status) {
            self.ready.onError(status);
        } else {
            self.ready.start(self.loop, res->ai_addr);
        }
    }
};

}

#endif
