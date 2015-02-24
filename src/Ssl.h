#ifndef BOT_SSL_H
#define BOT_SSL_H

#include <deque>
#include <polarssl/ssl.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/error.h>

#include "Promise.h"

namespace bot {

struct SslContext {
    int init();

    entropy_context entropy;
    x509_crt cacert;
    ctr_drbg_context ctr_drbg;
};

class SslConnection {
public:
    SslConnection(SslContext &ctx) : ctx(ctx) {}
    int init();
    int handshake();
    // raw TLS data
    int input(const uint8_t *data, size_t len);
    // application data
    int write(const uint8_t *data, size_t len);
    void cancel();

    Promise<std::pair<const uint8_t*, size_t>> output_promise;
    Promise<std::pair<const uint8_t*, size_t>> read_promise;
    Promise<void> connected_promise;

private:
    enum class State {
        Handshake,
        Verify,
        Data
    };

    static int recv(void *ptr, unsigned char *buf, size_t len);
    static int send(void *ptr, const unsigned char *buf, size_t len);
    std::deque<uint8_t> input_buf;
    SslContext &ctx;
    ssl_context ssl;
    State state = State::Handshake;
    uint8_t read_buf[65536];
};

}

#endif
