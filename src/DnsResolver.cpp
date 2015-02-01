#include "DnsResolver.h"

using namespace bot;

int DnsResolver::start(const char *node, const char *service, uv_loop_t *loop) {
    this->node = node;
    this->service = service;
    this->loop = loop;
    req.data = this;
    return uv_getaddrinfo(loop, &req, &DnsResolver::callback, node, service, NULL);
}

void DnsResolver::callback(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
    DnsResolver &self = *reinterpret_cast<DnsResolver*>(req->data);
    if (status) {
        self.error_promise.run(status);
    } else {
        self.addr_promise.run(res->ai_addr);
    }
}
