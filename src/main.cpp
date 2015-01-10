#include <uv.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <deque>
#include <cstring>

using namespace std;

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

struct IrcMessage {
    string nickname, user, host, command;
    vector<string> params;
    bool trailing = false;
};

string to_string(const IrcMessage &msg)
{
    auto acc = msg.nickname + "!" + msg.user + "@" + msg.host + " " + msg.command;
    if (!msg.params.empty()) {
        for (unsigned i = 0; i < msg.params.size() - 1; i++) {
            acc += msg.params[i] + " ";
        }
        if (msg.trailing) {
            acc += ":" + msg.params.back();
        } else {
            acc += msg.params.back();
        }
    }
    return acc;
}

class IrcParser {
public:
    void push(string &&str) {
        queue.emplace_back(str);
    }

    bool run(IrcMessage &out) {
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

private:
    struct fn;
    typedef fn (IrcParser::*fp)(char c);

    struct fn {
        fn(fp f) : f(f) {}
        operator fp() { return f; }
        fp f;
    };

    fn read_prefix(char c) {
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

    fn read_nickname(char c) {
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

    fn read_user(char c) {
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

    fn read_host(char c) {
        switch (c) {
        case ' ':
            return &IrcParser::read_command;
        default:
            cur.host.push_back(c);
            return &IrcParser::read_host;
        }
    }

    fn read_command(char c) {
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

    fn read_params(char c) {
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

    fn read_trailing(char c) {
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

    IrcMessage cur;
    deque<string> queue;
    unsigned str_pos = 0;
};

struct IrcServer {
    uv_tcp_t *tcp = nullptr;
    uv_loop_t *loop = nullptr;
    char buffer[65536] = "";
    deque<string> write_queue;
    unsigned write_num = 0;
    uv_write_t write_req;
    bool write_active = false;
    string nickname, user, realname;
    vector<string> channels_to_join;

    IrcServer(string nickname, string user, string realname, vector<string> &&channels)
        : nickname(nickname), user(user), realname(realname), channels_to_join(channels) {}

    int start(uv_loop_t *loop, uv_tcp_t *tcp) {
        this->loop = loop;
        this->tcp = tcp;
        tcp->data = this;
        int res;
        if ((res = uv_read_start(reinterpret_cast<uv_stream_t*>(tcp), &IrcServer::alloc, &IrcServer::on_read))) {
            return res;
        }
        writef("NICK %s\r\n", nickname.c_str());
        writef("USER %s 0 0 %s\r\n", user.c_str(), realname.c_str());
        return 0;
    }

    static void alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
        (void)size;
        IrcServer &self = *reinterpret_cast<IrcServer*>(handle->data);
        buf->base = self.buffer;
        buf->len = sizeof(self.buffer);
    }

    static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
        (void)stream;
        fwrite(buf->base, nread, 1, stdout);
    }

    void write(std::string &&str) {
        write_queue.emplace_back(str);
        if (!write_active) {
            vector<uv_buf_t> bufs;
            for (auto &str : write_queue) {
                bufs.emplace_back(uv_buf_t { const_cast<char*>(str.c_str()), str.size() });
            }
            write_active = true;
            uv_write(&write_req, reinterpret_cast<uv_stream_t*>(tcp), bufs.data(), bufs.size(),
                     &IrcServer::on_write);
            write_req.data = this;
        }
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
            printf(">>> %s\n", uv_strerror(status));
        } else {
            self.write_queue.clear();
        }
        self.write_active = false;
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

#ifndef TEST
int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    uv_loop_t loop;
    uv_loop_init(&loop);

    string name = "StormBot";
    vector<string> chans = {
        "#test"
    };
    IrcServer irc(name, name, name, move(chans));
    TcpConnection tcp(irc);
    DnsResolver resolver("irc.stormbit.net", "6667", tcp);

    resolver.start(&loop);

    uv_run(&loop, UV_RUN_DEFAULT);

    return 0;
}
#endif
