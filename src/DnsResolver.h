#ifndef BOT_DNS_RESOLVER_H
#define BOT_DNS_RESOLVER_H

#include <cstdio>
#include <uv.h>

#include "Util.h"
#include "Promise.h"
#include "Result.h"

namespace bot {

class DnsResolver {
public:
    DnsResolver() = default;
    DnsResolver(DnsResolver &) = delete;
    DnsResolver(DnsResolver &&) = delete;

    int start(const char *node, const char *service, uv_loop_t *loop);
    const char *getNode() const {
        return node;
    }

    Promise<struct sockaddr*> addr_promise;
    Promise<int> error_promise;

private:
    static void callback(uv_getaddrinfo_t *req, int status, struct addrinfo *res);

    uv_getaddrinfo_t req;
    const char *node = nullptr, *service = nullptr;
    uv_loop_t *loop = nullptr;
};

}

#endif
