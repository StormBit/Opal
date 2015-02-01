#include "TcpConnection.h"

using namespace bot;

void TcpConnection::start(uv_loop_t *loop, Promise<struct sockaddr *> &promise) {
    promise.then([=](struct sockaddr* addr) {
            int res = start(loop, addr);
            if (res) {
                onError(res);
            }
        });
}

int TcpConnection::start(uv_loop_t *loop, struct sockaddr *addr) {
    write_queue.clear();
    write_num = 0;
    write_active = false;
    this->loop = loop;
    int res;
    if ((res = uv_tcp_init(loop, &tcp)) != 0) {
        onError(res);
        return res;
    }
    tcp.data = this;
    if ((res = uv_tcp_connect(&req, &tcp, addr, &TcpConnection::callback))) {
        onError(res);
        return res;
    }
    req.data = this;
    active = true;
    return 0;
}

void TcpConnection::shutdown(std::function<void ()> &&callback) {
    if (!active) {
        return;
    }
    active = false;
    int res;
    if ((res = uv_read_stop(reinterpret_cast<uv_stream_t*>(&tcp)))) {
        onError(res);
    }
    if ((res = uv_shutdown(&shutdown_req, reinterpret_cast<uv_stream_t*>(&tcp), &TcpConnection::on_shutdown))) {
        onError(res);
    }
    shutdown_req.data = this;
    shutdown_cb = std::move(callback);
}

void TcpConnection::write(std::string &&str) {
    write_queue.emplace_back(str);
    beginWrite();
}

void TcpConnection::writef(const char *fmt, ...) {
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

void TcpConnection::onError(int status) {
    error_promise.run(status);
}

void TcpConnection::beginWrite() {
    if (!write_active && !write_queue.empty()) {
        std::vector<uv_buf_t> bufs;
        for (auto &str : write_queue) {
            bufs.emplace_back(uv_buf_t { const_cast<char*>(str.c_str()), str.size() });
        }
        write_num = write_queue.size();
        write_active = true;
        uv_write(&write_req, reinterpret_cast<uv_stream_t*>(&tcp), bufs.data(), bufs.size(),
                 &TcpConnection::on_write);
        write_req.data = this;
    }
}

void TcpConnection::callback(uv_connect_t *req, int status) {
    TcpConnection &self = *reinterpret_cast<TcpConnection*>(req->data);
    if (status) {
        self.onError(status);
        return;
    }
    int res;
    if ((res = uv_read_start(reinterpret_cast<uv_stream_t*>(&self.tcp),
                             &TcpConnection::alloc, &TcpConnection::on_read))) {
        self.onError(res);
        return;
    }
    self.connected_promise.run(&self.tcp);
}

void TcpConnection::alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    (void)size;
    TcpConnection &self = *reinterpret_cast<TcpConnection*>(handle->data);
    buf->base = self.buffer;
    buf->len = sizeof(self.buffer);
}

void TcpConnection::on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    TcpConnection &self = *reinterpret_cast<TcpConnection*>(stream->data);
    if (nread < 0) {
        self.onError(nread);
    } else {
        self.read_promise.run(uv_buf_t {buf->base, (size_t)nread});
    }
}

void TcpConnection::on_write(uv_write_t *req, int status) {
    TcpConnection &self = *reinterpret_cast<TcpConnection*>(req->data);
    if (status != 0) {
        self.onError(status);
    } else {
        for (unsigned i = 0; i < self.write_num; i++) {
            self.write_queue.pop_front();
        }
        self.write_num = 0;
    }
    self.write_active = false;
    self.beginWrite();
}

void TcpConnection::on_shutdown(uv_shutdown_t *req, int status) {
    TcpConnection &self = *reinterpret_cast<TcpConnection*>(req->data);
    if (status) {
        self.onError(status);
    }
    uv_close(reinterpret_cast<uv_handle_t*>(&self.tcp), &TcpConnection::on_close);
}

void TcpConnection::on_close(uv_handle_t *handle)
{
    TcpConnection &self = *reinterpret_cast<TcpConnection*>(handle->data);
    self.shutdown_cb();
}
