#ifndef BOT_CONNECTION_H
#define BOT_CONNECTION_H

#include "DnsResolver.h"
#include "TcpConnection.h"
#include "Tls.h"

namespace bot {

class Connection {
public:
    struct Error {
        enum class Type {
            Okay,
            Bot,
            Uv,
            Mbed,
            Cert
        };

        Error() : type(Type::Okay), code(0), source(""), msg() {}
        Error(Type type, int code, std::string &&msg, const char *source = "") : type(type), code(code), source(source), msg(std::move(msg)) {}

        bool is_ok() const {
            return type == Type::Okay;
        }

        static Error uv_error(const char *source, int code) {
            return Error(Type::Uv, code, uv_strerror(code), source);
        }

        static Error mbed_error(const char *source, int code) {
            char buf[1024];
            polarssl_strerror(code, buf, 1024);
            return Error(Type::Mbed, code, buf, source);
        }

        std::string to_string() const {
            const char *const names[] = { "okay", "bot", "libuv", "mbed-tls", "cert" };
            return std::string(source) + " error from " + names[static_cast<unsigned>(type)] + ": " + msg;
        }

        Type type;
        int code;
        const char *source;
        std::string msg;
    };
    Connection() = default;
    Connection(Connection&) = delete;
    Connection(Connection&&) = delete;

    Error start(uv_loop_t *loop, const char *node, const char *service, bool tls);
    void write(std::string &&str);
    void writef(const char *fmt, ...);

    Promise<Error> error_promise;
    Promise<void> connected_promise;
    Promise<uv_buf_t> read_promise;

private:
    void checkRet(const char *source, int code);

    static TlsContext context;
    static bool context_initialized;

    DnsResolver dns;
    TcpConnection tcp;
    TlsConnection tls;
    bool use_tls;
};

}

#endif
