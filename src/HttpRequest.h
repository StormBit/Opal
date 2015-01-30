#ifndef BOT_HTTP_REQUEST_H
#define BOT_HTTP_REQUEST_H

#include <string>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <uv.h>
#include <http_parser.h>

#include "LuaWrap.h"
#include "LuaRef.h"
#include "DnsResolver.h"
#include "TcpConnection.h"

namespace bot {

const char HttpRequestName[] = "HttpRequest";

class HttpRequest : public ObjectMixins<HttpRequest, HttpRequestName>{
public:
    enum Errno {
        URL_FAIL,
        MISSING_HOST,
    };

    struct HttpError {
        enum class Kind {
            Uv,
            Http,
            Status,
            Request
        };

        HttpError(Kind kind, int code, const char *msg) : kind(kind), code(code), msg(msg) {}

        const Kind kind;
        const int code;
        const char *msg;
    };

    HttpRequest() {
        memset(&settings, 0, sizeof(http_parser_settings));
        settings.on_message_begin = &HttpRequest::noop;
        settings.on_status = &HttpRequest::on_status;
        settings.on_header_field = &HttpRequest::on_header_field;
        settings.on_header_value = &HttpRequest::on_header_value;
        settings.on_body = &HttpRequest::on_body;
        http_parser_init(&parser, HTTP_RESPONSE);
        parser.data = this;
        request_headers.emplace("Accept-Charset", "UTF-8");
        request_headers.emplace("User-Agent", "OpalIRC");
    }

    bool begin(const char *url, uv_loop_t *loop);
    int start(uv_loop_t *loop, uv_tcp_t *addr);
    void cancel();

    void onRead(size_t nread, const uv_buf_t *buf);
    void onError(int status);

    http_method method = HTTP_GET;
    std::unordered_map<std::string, std::string> request_headers;
    std::unordered_map<std::string, std::string> response_headers;
    Promise<HttpError> error_promise;
    Promise<std::tuple<const char*, size_t>> response_promise;

    int __index(lua_State *L);
    static void openlib(lua_State *L);

private:
    static int noop(http_parser *parser);
    static int on_status(http_parser *parser, const char *buf, size_t length);
    static int on_header_field(http_parser *parser, const char *buf, size_t length);
    static int on_header_value(http_parser *parser, const char *buf, size_t length);
    static int on_body(http_parser *parser, const char *buf, size_t length);
    static int lua_new(lua_State *L);
    static int lua_cancel(lua_State *L);

    http_parser_url url;
    http_parser parser;
    http_parser_settings settings;
    TcpConnection tcpcon;
    DnsResolver dns;
    std::string host, service, path, current_header;
    uv_loop_t *loop = nullptr;
    uv_tcp_t *tcp = nullptr;
    LuaRef ref;
    bool cancelled = false, redirecting = false;
};

}

#endif
