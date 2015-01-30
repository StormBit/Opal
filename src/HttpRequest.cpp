#include "HttpRequest.h"

#include <cstring>

#include "LuaModule.h"

using namespace std;
using namespace bot;

bool HttpRequest::begin(const char *addr, uv_loop_t *loop)
{
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
    tcp.connected_promise.then([=](uv_tcp_t*) {
            tcp.writef("%s %s HTTP/1.1\r\n", http_method_str(method), path.c_str());
            tcp.writef("Host: %s\r\n", host.c_str());
            for (auto &kv : request_headers) {
                tcp.writef("%s: %s\r\n", kv.first.c_str(), kv.second.c_str());
            }
            tcp.writef("\r\n");
        });
    tcp.read_promise.then([=](uv_buf_t buf) {
            if (!cancelled) {
                http_parser_execute(&parser, &settings, buf.base, buf.len);
            }
        });
    tcp.start(loop, dns.addr_promise);
    if (int res = dns.start(host.c_str(), service.c_str(), loop)) {
        const char *err = http_errno_description(static_cast<enum http_errno>(res));
        error_promise.run(HttpError(HttpError::Kind::Http, res, err));
        return false;
    }
    return true;
}

void HttpRequest::cancel()
{
    tcp.shutdown([=](int) {
            ref.unref();
        });
    cancelled = true;
}

int HttpRequest::__index(lua_State *L)
{
    auto &self = reinterpret_cast<LuaWrap<HttpRequest>*>(luaL_checkudata(L, 1, HttpRequestName))->get();
    const char *key = luaL_checkstring(L, 2);

    const char *method = http_method_str(self.method);
    if (!strcmp(key, "method")) {
        lua_pushstring(L, method);
        return 1;
    }
    if (!strcmp(key, "cancel")) {
        lua_pushcfunction(L, &HttpRequest::lua_cancel);
        return 1;
    }
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
        self.redirecting = true; // start a new request
        printf("redirect start\n");
        break;
    default:
        // 304: NOT MODIFIED
        // 4xx: CLIENT ERROR
        // 5xx: SERVER ERROR
        if (code == 304 || code >= 400) {
            char err[1024];
            sprintf(err, "HTTP %li", code);
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
        self.cancelled = true;
        printf("redirect to: %s\n", str.c_str());
        self.tcp.shutdown([&self, str](int status) mutable {
                (void)status;
                printf("redirect shutdown\n");
                self.response_headers.clear();
                self.cancelled = false;
                self.begin(str.c_str(), self.loop);
            });
        return 1;
    }
    self.response_headers.emplace(std::move(self.current_header), std::move(str));
    return 0;
}

int HttpRequest::on_body(http_parser *parser, const char *buf, size_t length)
{
    auto &self = *reinterpret_cast<HttpRequest*>(parser->data);
    self.response_promise.run(make_tuple(buf, length));
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
    HttpRequest *req = new HttpRequest;

    string url;

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
        if (!strcmp(key, "headers")) {
            int t = lua_gettop(L);
            for (lua_pushnil(L); lua_next(L, t); lua_pop(L, 1)) {
                const char *key = luaL_checkstring(L, -2);
                const char *value = luaL_checkstring(L, -1);
                req->request_headers.emplace(key, value);
            }
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
                    lua_pushboolean(L, http_body_is_final(&req->parser));
                    if (lua_pcall(L, 3, 0, 0)) {
                        printf("http request callback: %s\n", lua_tostring(L, -1));
                    }
                });
        }
        if (!strcmp(key, "callback")) {
            lua_pushvalue(L, -1);
            auto ref = LuaRef::create(L);
            string buffer;
            req->response_promise.then([=](tuple<const char *, size_t> t) mutable {
                    const char *k = get<0>(t);
                    size_t l = get<1>(t);
                    buffer.append(k,l);
                    if (http_body_is_final(&req->parser)) {
                        ref.push();
                        wrap_object(req, L);
                        lua_pushlstring(L, buffer.data(), buffer.size());
                        if (lua_pcall(L, 2, 0, 0)) {
                            printf("http request callback: %s\n", lua_tostring(L, -1));
                        }
                    }
                });
        }
    }

    if (url.empty()) {
        luaL_error(L, "Must provide a URL");
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
