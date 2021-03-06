#include "HttpRequest.h"

#include <cstring>
#include <memory>

#include "LuaModule.h"

using namespace std;
using namespace bot;

HttpRequest::HttpRequest() {
    memset(&settings, 0, sizeof(http_parser_settings));
    settings.on_message_begin = &HttpRequest::noop;
    settings.on_status = &HttpRequest::on_status;
    settings.on_header_field = &HttpRequest::on_header_field;
    settings.on_header_value = &HttpRequest::on_header_value;
    settings.on_headers_complete = &HttpRequest::on_headers_complete;
    settings.on_body = &HttpRequest::on_body;
    settings.on_message_complete = &HttpRequest::on_message_complete;
    request_headers.emplace("Accept-Charset", "UTF-8");
    request_headers.emplace("User-Agent", "OpalIRC");
    dns.addr_promise.then([=](struct sockaddr *addr) {
            tcp.start(loop, addr);
        });
    dns.error_promise.then([=](int err) {
            error_promise.run(HttpError(HttpError::Kind::Uv, err, uv_strerror(err)));
        });
    tcp.connected_promise.then([=](uv_tcp_t*) {
            tcp.writef("%s %s HTTP/1.1\r\n", http_method_str(method), path.c_str());
            tcp.writef("Host: %s\r\n", host.c_str());
            // not technically necessary, but we can't currently reuse keepalive'd connections
            tcp.writef("Connection: Close\r\n");
            if (!payload.empty()) {
                tcp.writef("Content-Length: %zu\r\n", payload.size());
            }
            for (auto &kv : request_headers) {
                tcp.writef("%s: %s\r\n", kv.first.c_str(), kv.second.c_str());
            }
            tcp.writef("\r\n");
            if (!payload.empty()) {
                tcp.write(move(payload));
                tcp.writef("\r\n");
            }
        });
    tcp.read_promise.then([=](uv_buf_t buf) {
            if (!cancelled) {
                http_parser_execute(&parser, &settings, buf.base, buf.len);
            }
        });
    tcp.error_promise.then([=](int err) {
            if (err == UV_EOF) {
                finish_promise.run();
                return;
            }
            error_promise.run(HttpError(HttpError::Kind::Uv, err, uv_strerror(err)));
        });
    timer.promise.then([=]() {
            error_promise.run(HttpError(HttpError::Kind::Request, Errno::TIMEOUT, "Connection timed out"));
            cancel();
        });
    finish_promise.then([=]() {
            timer.stop();
        });
}

bool HttpRequest::begin(const char *addr, uv_loop_t *loop)
{
    http_parser_init(&parser, HTTP_RESPONSE);
    parser.data = this;
    this->loop = loop;
    if (http_parser_parse_url(addr, strlen(addr), false, &url)) {
        const char *err = "Parsing URL failed";
        error_promise.run(HttpError(HttpError::Kind::Request, Errno::URL_FAIL, err));
        return false;
    }
    if (!(url.field_set & (1 << UF_HOST))) {
        const char *err = "URL is missing host component";
        error_promise.run(HttpError(HttpError::Kind::Request, Errno::MISSING_HOST, err));
        return false;
    }
    if (!(url.field_set & (1 << UF_PATH))) {
        path = "/";
    } else {
        path = string(addr + url.field_data[UF_PATH].off, url.field_data[UF_PATH].len);
    }
    if (!url.port) {
        service = "http";
    } else {
        service = string(addr + url.field_data[UF_PORT].off, url.field_data[UF_PORT].len);
    }
    host = string(addr + url.field_data[UF_HOST].off, url.field_data[UF_HOST].len);
    if (url.field_set & (1 << UF_QUERY)) {
        path += "?" + string(addr + url.field_data[UF_QUERY].off, url.field_data[UF_QUERY].len);
    }
    if (int res = dns.start(host.c_str(), service.c_str(), loop)) {
        const char *err = http_errno_description(static_cast<enum http_errno>(res));
        error_promise.run(HttpError(HttpError::Kind::Http, res, err));
        return false;
    }
    timer.start(loop, 15 * 1000); // 15 seconds is a decent timeout
    return true;
}

void HttpRequest::cancel()
{
    if (!cancelled) {
        struct x {
            void dec() {
                count--;
                if (count < 1) {
                    ref.unref();
                }
            }

            int count;
            LuaRef ref;
        };
        shared_ptr<x> x(new struct x);
        x->count = 2;
        x->ref = ref;
        tcp.shutdown([=]() {
                x->dec();
            });
        timer.stop();
        timer.close([=]() {
                x->dec();
            });
        cancelled = true;
    }
}

void HttpRequest::restart(string &&url)
{
    cancelled = true;
    struct x {
        x(int count, LuaRef ref, HttpRequest &req, string &&url)
            : count(count), ref(ref), req(req), url(move(url)) {}

        void dec() {
            count--;
            if (count < 1) {
                req.response_headers.clear();
                req.cancelled = false;
                req.begin(url.c_str(), req.loop);
            }
        }

        int count;
        LuaRef ref;
        HttpRequest &req;
        std::string url;
    };
    shared_ptr<x> x(new struct x(2, ref, *this, move(url)));
    tcp.shutdown([=]() {
            x->dec();
        });
    timer.stop();
    timer.close([=]() {
            x->dec();
        });
}

int HttpRequest::__index(lua_State *L)
{
    const char *key = luaL_checkstring(L, 2);

    const char *method = http_method_str(this->method);
    if (!strcmp(key, "method")) {
        lua_pushstring(L, method);
        return 1;
    }
    if (!strcmp(key, "payload")) {
        lua_pushlstring(L, payload.data(), payload.size());
        return 1;
    }
    if (!strcmp(key, "cancel")) {
        lua_pushcfunction(L, &HttpRequest::lua_cancel);
        return 1;
    }
    if (!strcmp(key, "responseheaders")) {
        lua_newtable(L);
        for (auto &kv : response_headers) {
            lua_pushlstring(L, kv.first.data(), kv.first.size());
            lua_pushlstring(L, kv.second.data(), kv.second.size());
            lua_settable(L, -3);
        }
        return 1;
    }
    return 0;
}

int HttpRequest::__newindex(lua_State *L)
{
    const char *key = luaL_checkstring(L, 2);

    if (!strcmp(key, "payload")) {
        size_t len;
        const char *str = lua_tolstring(L, 3, &len);
        payload = string(str, len);
    }

    luaL_error(L, "Invalid key %s", key);
    return 0;
}

void HttpRequest::openlib(lua_State *L)
{
    lua_newtable(L);
    lua_pushstring(L, "new");
    lua_pushcfunction(L, &HttpRequest::lua_new);
    lua_settable(L, -3);
    lua_setglobal(L, "httprequest");
}

int HttpRequest::noop(http_parser *parser)
{
    (void)parser;
    return 0;
}

int HttpRequest::on_status(http_parser *parser, const char *buf, size_t length)
{
    (void)buf, (void)length;
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    long code = parser->status_code;
    switch (code) {
    case 200: // OK
    case 201: // CREATED
    case 206: // PARTIAL CONTENT
        break; // continue as normal
    case 301: // MOVED PERMANENTLY
    case 302: // FOUND
    case 303: // SEE OTHER
    case 307: // TEMPORARY REDIRECT
    case 308: // PERMANENT REDIRECT
        self.redirect_count++;
        if (self.redirect_count > 5) {
            self.error_promise.run(HttpError(HttpError::Kind::Request, Errno::REDIRECT_LOOP, "Redirection loop"));
        } else {
            self.redirecting = true; // start a new request
        }
        break;
    default:
        // 304: NOT MODIFIED
        // 4xx: CLIENT ERROR
        // 5xx: SERVER ERROR
        if (code == 304 || code >= 400) {
            char err[1024];
            self.error_promise.run(HttpError(HttpError::Kind::Status, code, err));
        }
        // ignore everything else
        break;
    }
    return 0;
}

int HttpRequest::on_header_field(http_parser *parser, const char *buf, size_t length)
{
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    self.current_header = string(buf, length);
    return 0;
}

int HttpRequest::on_header_value(http_parser *parser, const char *buf, size_t length)
{
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    string str(buf, length);
    if (self.redirecting && self.current_header == "Location") {
        self.redirecting = false;
        self.restart(move(str));
        return 1;
    }
    self.response_headers.emplace(std::move(self.current_header), std::move(str));
    return 0;
}

int HttpRequest::on_headers_complete(http_parser *parser)
{
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    self.started_promise.run();
    return 0;
}

int HttpRequest::on_body(http_parser *parser, const char *buf, size_t length)
{
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    if (!self.cancelled) {
        self.response_promise.run(make_tuple(buf, length));
    }
    return 0;
}

int HttpRequest::on_message_complete(http_parser *parser)
{
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    self.finish_promise.run();
    self.cancel();
    return 0;
}

static int method_from_string(const char *str)
{
#define XX(num, name, string) #string,
    static const char *const map[] = {
        HTTP_METHOD_MAP(XX)
        NULL
    };
#undef XX
    for (unsigned i = 0; map[i]; i++) {
        if (!strcasecmp(map[i], str)) {
            return i;
        }
    }
    return -1;
}

int HttpRequest::lua_new(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    HttpRequest *req = new HttpRequest();

    string url;

    bool have_error_cb = false;

    for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1)) {
        const char *key = luaL_checkstring(L, -2);
        if (!strcmp(key, "url")) {
            url = luaL_checkstring(L, -1);
        }
        if (!strcmp(key, "method")) {
            int res = method_from_string(luaL_checkstring(L, -1));
            if (res < 0) {
                luaL_error(L, "Invalid value for http_method enum");
                return 0;
            }
            req->method = static_cast<enum http_method>(res);
        }
        if (!strcmp(key, "payload")) {
            size_t len;
            const char *str = luaL_checklstring(L, -1, &len);
            req->payload = string(str, len);
        }
        if (!strcmp(key, "headers")) {
            int t = lua_gettop(L);
            for (lua_pushnil(L); lua_next(L, t); lua_pop(L, 1)) {
                const char *key = luaL_checkstring(L, -2);
                const char *value = luaL_checkstring(L, -1);
                req->request_headers.emplace(key, value);
            }
        }
        if (!strcmp(key, "started")) {
            lua_pushvalue(L, -1);
            auto ref = LuaRef::create(L);
            req->started_promise.then([=]() {
                    ref.push();
                    wrap_object(req, L);
                    if (lua_pcall(L, 1, 0, 0)) {
                        printf("http request callback: %s\n", lua_tostring(L, -1));
                    }
                });
        }
        if (!strcmp(key, "unbuffered")) {
            lua_pushvalue(L, -1);
            auto ref = LuaRef::create(L);
            req->response_promise.then([=](tuple<const char *, size_t> t) {
                    const char *k = get<0>(t);
                    size_t l = get<1>(t);
                    ref.push();
                    wrap_object(req, L);
                    lua_pushlstring(L, k, l);
                    if (lua_pcall(L, 2, 0, 0)) {
                        printf("http request callback: %s\n", lua_tostring(L, -1));
                    }
                });
        }
        if (!strcmp(key, "finished")) {
            lua_pushvalue(L, -1);
            auto ref = LuaRef::create(L);
            req->finish_promise.then([=]() {
                    ref.push();
                    wrap_object(req, L);
                    if (lua_pcall(L, 0, 0, 0)) {
                        printf("http request callback: %s\n", lua_tostring(L, -1));
                    }
                });
        }
        if (!strcmp(key, "callback")) {
            lua_pushvalue(L, -1);
            struct ctx {
                LuaRef ref;
                string buffer;
            };
            shared_ptr<ctx> ctx = shared_ptr<struct ctx>(new struct ctx);
            ctx->ref = LuaRef::create(L);
            req->response_promise.then([=](tuple<const char *, size_t> t) mutable {
                    const char *k = get<0>(t);
                    size_t l = get<1>(t);
                    ctx->buffer.append(k,l);
                });
            req->finish_promise.then([=]() {
                    ctx->ref.push();
                    wrap_object(req, L);
                    lua_pushlstring(L, ctx->buffer.data(), ctx->buffer.size());
                    if (lua_pcall(L, 2, 0, 0)) {
                        printf("http request callback: %s\n", lua_tostring(L, -1));
                    }
                });
        }
        if (!strcmp(key, "error")) {
            have_error_cb = true;
            lua_pushvalue(L, -1);
            auto ref = LuaRef::create(L);
            req->error_promise.then([=](HttpError err) {
                    static const char *const kinds[] = {
                        "uv", "http", "status", "request"
                    };
                    ref.push();
                    wrap_object(req, L);
                    lua_pushstring(L, err.msg);
                    lua_pushstring(L, kinds[static_cast<int>(err.kind)]);
                    lua_pushinteger(L, err.code);
                    if (lua_pcall(L, 4, 0, 0)) {
                        printf("http error callback: %s\n", lua_tostring(L, -1));
                    }
                });
        }
    }

    if (url.empty()) {
        luaL_error(L, "Must provide a URL");
    }

    if (!have_error_cb) {
        req->error_promise.then([=](HttpError err) {
                printf("http request: %s\n", err.msg);
            });
    }

    uv_loop_t *loop = LuaModule::getModule(L).loop;
    req->begin(url.c_str(), loop);

    LuaWrap<IObject>::create(L, unique_ptr<IObject>(static_cast<IObject*>(req)));
    lua_pushvalue(L, -1);
    req->ref = LuaRef::create(L);
    return 1;
}

int HttpRequest::lua_cancel(lua_State *L)
{
    auto &self = unwrap(L);
    self.cancel();
    return 0;
}
