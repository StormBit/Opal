#ifndef BOT_TCP_CONNECTION_H
#define BOT_TCP_CONNECTION_H

#include <deque>
#include <string>
#include <vector>
#include <functional>
#include <uv.h>
#include <cassert>

#include "Promise.h"
#include "Result.h"

namespace bot {

class TcpConnection {
public:
    TcpConnection() = default;
    TcpConnection(TcpConnection &) = delete;
    TcpConnection(TcpConnection &&) = delete;

    void start(uv_loop_t *loop, Promise<struct sockaddr*> &promise);
    int start(uv_loop_t *loop, struct sockaddr *addr);
    void shutdown(std::function<void ()> &&callback);
    void write(std::string &&str);
    void writef(const char *fmt, ...);

    Promise<int> error_promise;
    Promise<uv_tcp_t*> connected_promise;
    Promise<uv_buf_t> read_promise;

private:
    void onError(int status);
    void beginWrite();
    static void callback(uv_connect_t *req, int status);
    static void alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf);
    static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void on_write(uv_write_t *req, int status);
    static void on_shutdown(uv_shutdown_t *req, int status);
    static void on_close(uv_handle_t *handle);

    char buffer[65536] = "";
    std::deque<std::string> write_queue;
    unsigned write_num = 0;
    uv_write_t write_req;
    uv_shutdown_t shutdown_req;
    std::function<void ()> shutdown_cb;
    bool write_active = false;
    uv_tcp_t tcp;
    uv_connect_t req;
    uv_loop_t *loop = nullptr;
};

}

#endif
