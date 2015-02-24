#include "Ssl.h"

using namespace std;
using namespace bot;

int SslContext::init()
{
    x509_crt_init(&cacert);
    entropy_init(&entropy);
    x509_crt_parse_path(&cacert, "/etc/ssl/certs"); // ??? why the fuck do I have to hardcode this?
    int ret;
    static const unsigned char pers[] = "Opal IRC";
    if ((ret = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy, pers, sizeof(pers))) != 0) {
        return ret;
    }
    return 0;
}

static void my_debug(void *ctx, int level, const char *str)
{
    (void)ctx;
    fprintf(stderr, "mbed-tls %i: %s", level, str);
}

int SslConnection::init()
{
    memset(&ssl, 0, sizeof(ssl_context));
    int ret = ssl_init(&ssl);
    if (ret != 0) {
        return ret;
    }
    ssl_set_endpoint(&ssl, SSL_IS_CLIENT);
    ssl_set_authmode(&ssl, SSL_VERIFY_OPTIONAL);
    ssl_set_ca_chain(&ssl, &ctx.cacert, NULL, NULL);
    ssl_set_min_version(&ssl, SSL_MAJOR_VERSION_3, SSL_MINOR_VERSION_1);
    ssl_set_arc4_support(&ssl, SSL_ARC4_DISABLED);
    ssl_set_rng(&ssl, ctr_drbg_random, &ctx.ctr_drbg);
    ssl_set_dbg(&ssl, my_debug, NULL);
    ssl_set_bio(&ssl,
                &SslConnection::recv, this,
                &SslConnection::send, this);
    return 0;
}

int SslConnection::handshake()
{
    int ret;
    switch (state) {
    case State::Handshake:
        if ((ret = ssl_handshake(&ssl)) != 0) {
            return ret;
        }
        state = State::Verify;
        [[clang::fallthrough]];
    case State::Verify:
        if ((ret = ssl_get_verify_result(&ssl)) != 0) {
            return ret;
        }
        state = State::Data;
        connected_promise.run();
        [[clang::fallthrough]];
    case State::Data:
        switch ((ret = ssl_read(&ssl, read_buf, sizeof(read_buf)))) {
        case POLARSSL_ERR_NET_WANT_READ:
        case POLARSSL_ERR_NET_WANT_WRITE:
            return ret;
        default:
            if (ret < 0) {
                return ret;
            }
        }
        read_promise.run(make_pair(read_buf, ret));
        return 0;
    }
}

int SslConnection::input(const uint8_t *data, size_t len)
{
    for (unsigned i = 0; i < len; i++) {
        input_buf.push_back(data[i]);
    }
    return handshake();
}

int SslConnection::write(const uint8_t *data, size_t len)
{
    return ssl_write(&ssl, data, len);
}

void SslConnection::cancel()
{
    ssl_close_notify(&ssl);
}

int SslConnection::recv(void *ptr, unsigned char *buf, size_t len)
{
    SslConnection &self = *reinterpret_cast<SslConnection*>(ptr);
    if (self.input_buf.empty()) {
        return POLARSSL_ERR_NET_WANT_READ;
    }
    size_t size = min(len, self.input_buf.size());
    for (unsigned i = 0; i < size; i++) {
        buf[i] = self.input_buf[i];
    }
    self.input_buf.erase(self.input_buf.begin(), self.input_buf.begin() + size);
    return size;
}

int SslConnection::send(void *ptr, const unsigned char *buf, size_t len)
{
    SslConnection &self = *reinterpret_cast<SslConnection*>(ptr);
    self.output_promise.run(make_pair(buf, len));
    return len;
}
