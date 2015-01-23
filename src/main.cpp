#include <uv.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <deque>
#include <cstring>

#include "IrcParser.h"
#include "LuaModule.h"
#include "EventBus.h"
#include "IrcServer.h"

using namespace std;
using namespace bot;

struct TcpConnection {
    uv_tcp_t tcp;
    uv_connect_t req;
    uv_loop_t *loop = nullptr;
    IrcServer &irc;

    TcpConnection(IrcServer &irc) : irc(irc) {}

    int start(uv_loop_t *loop, struct sockaddr *addr) {
        this->loop = loop;
        int res;
        if ((res = uv_tcp_init(loop, &tcp)) != 0) {
            return res;
        }
        if ((res = uv_tcp_connect(&req, &tcp, addr, &TcpConnection::callback))) {
            return res;
        }
        req.data = this;
        return 0;
    }

    static void callback(uv_connect_t *req, int status) {
        (void)status;
        TcpConnection &self = *reinterpret_cast<TcpConnection*>(req->data);
        self.irc.start(self.loop, &self.tcp);
    }
};

struct DnsResolver {
    uv_getaddrinfo_t req;
    const char *node, *service;
    uv_loop_t *loop = nullptr;
    TcpConnection &tcp;

    DnsResolver(const char *node, const char *service, TcpConnection &tcp)
        : node(node), service(service), tcp(tcp) {}

    int start(uv_loop_t *loop) {
        this->loop = loop;
        return uv_getaddrinfo(loop, &req, &DnsResolver::callback, node, service, NULL);
    }

    static void callback(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
        (void)status;
        DnsResolver &self = *reinterpret_cast<DnsResolver*>(req);
        printf("addr: %s\n", to_string(res).c_str());
        self.tcp.start(self.loop, res->ai_addr);
    }
};

void print(const string &str)
{
    printf("%s\n", str.c_str());
}

#ifndef TEST
int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    uv_loop_t loop;
    uv_loop_init(&loop);

    EventBus bus;
    LuaModule test("test");
    test.load();
    test.openlib(bus);
    test.run();

    string name = "Opal";
    string addr = "irc.stormbit.net";
    vector<string> chans = {
        "#test"
    };
    IrcServer irc(addr, name, name, name, move(chans), bus);
    TcpConnection tcp(irc);
    DnsResolver resolver(addr.c_str(), "6667", tcp);

    resolver.start(&loop);

    uv_run(&loop, UV_RUN_DEFAULT);

    return 0;
}
#endif
