#include <uv.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <deque>
#include <cstring>

#include "IrcParser.h"
#include "LuaModule.h"
#include "EventBus.h"

using namespace std;
using namespace bot;

string to_string(struct addrinfo *addr)
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

string to_string(const IrcMessage &msg)
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

struct IrcServer {
    uv_tcp_t *tcp = nullptr;
    uv_loop_t *loop = nullptr;
    char buffer[65536] = "";
    deque<string> write_queue;
    unsigned write_num = 0;
    uv_write_t write_req;
    bool write_active = false;
    string address, nickname, user, realname;
    vector<string> channels_to_join;
    IrcParser parser;
    EventBus &bus;
    char prefix;

    IrcServer(string address, string nickname, string user, string realname, vector<string> &&channels,
              EventBus &bus, char prefix = '+')
        : address(address), nickname(nickname), user(user), realname(realname), channels_to_join(channels),
          bus(bus), prefix(prefix) {}

    int start(uv_loop_t *loop, uv_tcp_t *tcp) {
        this->loop = loop;
        this->tcp = tcp;
        tcp->data = this;
        int res;
        if ((res = uv_read_start(reinterpret_cast<uv_stream_t*>(tcp), &IrcServer::alloc, &IrcServer::on_read))) {
            return res;
        }
        writef("NICK :%s\r\n", nickname.c_str());
        writef("USER %s 0 0 :%s\r\n", user.c_str(), realname.c_str());
        return 0;
    }

    static void alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
        (void)size;
        IrcServer &self = *reinterpret_cast<IrcServer*>(handle->data);
        buf->base = self.buffer;
        buf->len = sizeof(self.buffer);
    }

    static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
        IrcServer &self = *reinterpret_cast<IrcServer*>(stream->data);
        if (nread < 0) {
            printf("xxx uv read: %s\n", uv_strerror(nread));
            return;
        }
        self.parser.push(string(buf->base, nread));
        IrcMessage msg;
        while (self.parser.run(msg)) {
            self.handleMessage(msg);
        }
    }

    void begin_write() {
        if (!write_active && !write_queue.empty()) {
            vector<uv_buf_t> bufs;
            for (auto &str : write_queue) {
                bufs.emplace_back(uv_buf_t { const_cast<char*>(str.c_str()), str.size() });
            }
            write_num = write_queue.size();
            write_active = true;
            uv_write(&write_req, reinterpret_cast<uv_stream_t*>(tcp), bufs.data(), bufs.size(),
                     &IrcServer::on_write);
            write_req.data = this;
        }
    }

    void write(std::string &&str) {
        write_queue.emplace_back(str);
        begin_write();
    }

    void writef(const char *fmt, ...) __attribute((format(printf, 2, 3))) {
        va_list ap1, ap2;
        va_start(ap1, fmt);
        ssize_t len = vsnprintf(NULL, 0, fmt, ap1);
        va_end(ap1);
        assert(len >= 0);
        char *buf = new char[len + 1];
        va_start(ap2, fmt);
        vsnprintf(buf, len+1, fmt, ap2);
        va_end(ap2);
        write(buf);
        delete[] buf;
    }

    static void on_write(uv_write_t *req, int status) {
        IrcServer &self = *reinterpret_cast<IrcServer*>(req->data);
        if (status != 0) {
            printf("xxx %s\n", uv_strerror(status));
        } else {
            for (unsigned i = 0; i < self.write_num; i++) {
                self.write_queue.pop_front();
            }
            self.write_num = 0;
        }
        self.write_active = false;
        self.begin_write();
    }

    void handleMessage(const IrcMessage &msg) {
        printf("<<< %s\n", to_string(msg).c_str());
        if (msg.command == "001") {
            bus.fire("server-connect", EventBus::Value(address));
            for (auto &c : channels_to_join) {
                writef("JOIN :%s\r\n", c.c_str());
            }
            return;
        }
        if (msg.command == "PING") {
            string v;
            for (unsigned i = 0; i < msg.params.size(); i++) {
                v += msg.params[i] + " ";
            }
            v.pop_back();
            writef("PONG :%s\r\n", v.c_str());
            return;
        }
        if (msg.command == "PRIVMSG") {
            if (msg.params.size() < 2) {
                return;
            }
            if (nickname == msg.params[0]) {
                handleCommand(msg.nickname, msg.nickname, msg.params[0], msg.params[1]);
                return;
            }
            if (msg.params[1][0] == prefix) {
                auto &args = msg.params[1];
                size_t split = args.find(" ");
                string tail;
                if (split != string::npos) {
                    tail = args.substr(split+1, string::npos);
                }
                string head = args.substr(1, split == string::npos? string::npos : split-1);
                handleCommand(msg.nickname, msg.params[0], head, tail);
                return;
            }
        }
    }

    void handleCommand(const string &nick, const string &chan, const string &cmd, const string &args) {
        std::unique_ptr<EventBus::Value::Table> table(new EventBus::Value::Table {
                {"server", address},
                {"nick", nick},
                {"channel", chan},
                {"command", cmd},
                {"args", args}
            });
        bus.fire("command", std::move(table));
        if (cmd == "echo") {
            writef("PRIVMSG %s :%s\r\n", chan.c_str(), args.c_str());
            return;
        }
    }
};

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

    string name = "StormBot";
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
