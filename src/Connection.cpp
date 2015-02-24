#include "Connection.h"

using namespace bot;
using namespace std;

TlsContext Connection::context;
bool Connection::context_initialized = false;

Connection::Error Connection::start(uv_loop_t *loop, const char *node, const char *service, bool use_tls)
{
    this->use_tls = use_tls;
    int res;

    tcp.start(loop, dns.addr_promise);
    if ((res = dns.start(node, service, loop)) != 0) {
        return Error(Error::Type::Uv, res, uv_strerror(res));
    }
    dns.error_promise.then([&](int code) {
            error_promise.run(Error::uv_error("dns resolution", code));
        });
    tcp.error_promise.then([&](int code) {
            error_promise.run(Error::uv_error("tcp", code));
        });
    if (use_tls) {
        if (!context_initialized) {
            if ((res = context.init()) != 0) {
                return Error::mbed_error("context initialization", res);
            }
            context_initialized = true;
        }
        tls.init(context);
        dns.addr_promise.then([&](struct sockaddr*) {
                auto p = tls.handshake();
                checkRet("handshake", p.first);
            });
        tcp.read_promise.then([&](uv_buf_t buf) {
                auto p = tls.input(reinterpret_cast<const uint8_t*>(buf.base), buf.len);
                int ret = p.first;
                int cert = p.second;
                checkRet("input", ret);
                string error;
                if (cert & BADCERT_EXPIRED) {
                    error += "expired, ";
                }
                if (cert & BADCERT_REVOKED) {
                    error += "revoked, ";
                }
                if (cert & BADCERT_CN_MISMATCH) {
                    error += "CN mismatch, ";
                }
                if (cert & BADCERT_NOT_TRUSTED) {
                    error += "self-signed, ";
                }
                if (!error.empty()) {
                    error.pop_back(); error.pop_back();
                    error_promise.run(Error(Error::Type::Cert, cert, move(error), "cert validation"));
                }
            });
        tls.output_promise.then([&](pair<const uint8_t*, size_t> p) {
                string str(reinterpret_cast<const char*>(p.first), p.second);
                tcp.write(move(str));
            });
        tls.read_promise.then([&](pair<const uint8_t*, size_t> p) {
                uv_buf_t buf {const_cast<char*>(reinterpret_cast<const char*>(p.first)), p.second};
                read_promise.run(buf);
            });
        tls.connected_promise.then([&]() {
                connected_promise.run();
            });
    } else {
        tcp.read_promise.then([&](uv_buf_t buf) {
                read_promise.run(buf);
            });
        tcp.connected_promise.then([&](uv_tcp_t *) {
                connected_promise.run();
            });
    }

    return Error();
}

void Connection::write(string &&str) {
    if (use_tls) {
        tls.write(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }
}

void Connection::writef(const char *fmt, ...)
{
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

void Connection::checkRet(const char *source, int code)
{
    if (code != 0 &&
        code != POLARSSL_ERR_NET_WANT_READ &&
        code != POLARSSL_ERR_NET_WANT_WRITE) {
        error_promise.run(Error::mbed_error(source, code));
    }
}
