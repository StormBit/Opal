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
#include "Timer.h"

namespace bot {

const char HttpRequestName[] = "HttpRequest";

class HttpRequest : public ObjectMixins<HttpRequest, HttpRequestName>{
public:
    enum Errno {
        URL_FAIL,
        MISSING_HOST,
        REDIRECT_LOOP,
        TIMEOUT
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

    HttpRequest();
    ~HttpRequest() {
        printf("~HttpRequest\n");
    }

    bool begin(const char *url, uv_loop_t *loop);
    void cancel();

    http_method method = HTTP_GET;
    std::unordered_map<std::string, std::string> request_headers;
    std::unordered_map<std::string, std::string> response_headers;
    Promise<HttpError> error_promise;
    Promise<std::tuple<const char*, size_t>> response_promise;
    Promise<void> started_promise, finish_promise;

    int __index(lua_State *L);
    static void openlib(lua_State *L);

private:
    void restart(std::string &&url);

    static int noop(http_parser *parser);
    static int on_status(http_parser *parser, const char *buf, size_t length);
    static int on_header_field(http_parser *parser, const char *buf, size_t length);
    static int on_header_value(http_parser *parser, const char *buf, size_t length);
    static int on_headers_complete(http_parser *parser);
    static int on_body(http_parser *parser, const char *buf, size_t length);
    static int on_message_complete(http_parser *parser);
    static int lua_new(lua_State *L);
    static int lua_cancel(lua_State *L);

    http_parser_url url;
    http_parser parser;
    http_parser_settings settings;
    TcpConnection tcp;
    DnsResolver dns;
    Timer timer;
    std::string host, service, path, current_header;
    uv_loop_t *loop = nullptr;
    LuaRef ref;
    bool cancelled = false, redirecting = false;
    unsigned redirect_count = 0;
};

}

#endif
