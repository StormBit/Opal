#include <gtest/gtest.h>

#include "DnsResolver.cpp"
#include "TcpConnection.cpp"
#include "Ssl.cpp"

TEST(Ssl, BasicClient)
{
    SslContext ctx;
    ctx.init();
    SslConnection ssl(ctx);
    ASSERT_EQ(ssl.init(), 0);
    DnsResolver dns;
    TcpConnection tcp;
    uv_loop_t loop;
    uv_loop_init(&loop);
    tcp.start(&loop, dns.addr_promise);
    dns.start("httpbin.org", "https", &loop);
    dns.addr_promise.then([&](struct sockaddr*) {
            ssl.handshake();
        });
    tcp.read_promise.then([&](uv_buf_t buf) {
            int ret = ssl.input(reinterpret_cast<const uint8_t*>(buf.base), buf.len);
            if (ret != 0 && ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
                char buf[1024];
                polarssl_strerror(ret, buf, 1024);
                fprintf(stderr, "ssl.input: %s\n", buf);
            }
        });
    ssl.output_promise.then([&](pair<const uint8_t*, size_t> p) {
            string str(reinterpret_cast<const char*>(p.first), p.second);
            tcp.write(move(str));
        });
    ssl.read_promise.then([&](pair<const uint8_t*, size_t> p) {
            string str(reinterpret_cast<const char*>(p.first), p.second);
            printf("%s", str.c_str());
        });
    ssl.connected_promise.then([&]() {
            static const uint8_t data[] = "GET /get HTTP/1.0\r\nHost: httpbin.org\r\n\r\n";
            ssl.write(data, sizeof(data));
        });
    uv_run(&loop, UV_RUN_DEFAULT);
}
