#include "HttpRequest.h"

#include <cstring>

#include "LuaModule.h"

using namespace std;
using namespace bot;

bool HttpRequest::begin(const char *addr, uv_loop_t *loop)
{
    if (http_parser_parse_url(addr, strlen(addr), false, &url)) {
        const char *err = "Parsing URL failed";
        response_callback(UV_UNKNOWN, err, strlen(err));
        return false;
    }
    if (!(url.field_set & (1 << UF_HOST))) {
        const char *err = "URL is missing host component";
        response_callback(UV_UNKNOWN, err, strlen(err));
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
        response_callback(res, err, strlen(err));
        return false;
    }
    return true;
}

int HttpRequest::start(uv_loop_t *loop, uv_tcp_t *tcp)
{
    this->loop = loop;
    this->tcp = tcp;
    tcpcon.writef("%s %s HTTP/1.1\r\n", http_method_str(method), path.c_str());
    tcpcon.writef("Host: %s\r\n", host.c_str());
    for (auto &kv : request_headers) {
        tcpcon.writef("%s: %s\r\n", kv.first.c_str(), kv.second.c_str());
    }
    tcpcon.writef("\r\n");
    return 0;
}

void HttpRequest::cancel()
{
    tcpcon.shutdown([=](int status) {
            (void)status;
            ref.unref();
        });
    cancelled = true;
}

void HttpRequest::onRead(size_t nread, const uv_buf_t *buf)
{
    if (!cancelled) {
        http_parser_execute(&parser, &settings, buf->base, nread);
    }
}

void HttpRequest::onError(int status)
{
    const char *error(uv_strerror(status));
    response_callback(status, error, strlen(error));
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
            size_t len = sprintf(err, "HTTP %li", code);
            self.response_callback(UV_UNKNOWN, err, len);
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
        self.tcpcon.shutdown([=](int status) mutable {
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
    self.response_callback(0, buf, length);
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
            req->response_callback = [=](int r, const char *k, size_t l) {
                ref.push();
                wrap_object(req, L);
                lua_pushboolean(L, r == 0);
                lua_pushlstring(L, k, l);
                lua_pushboolean(L, http_body_is_final(&req->parser));
                if (lua_pcall(L, 4, 0, 0)) {
                    printf("http request callback: %s\n", lua_tostring(L, -1));
                }
            };
        }
        if (!strcmp(key, "callback")) {
            lua_pushvalue(L, -1);
            auto ref = LuaRef::create(L);
            string buffer;
            req->response_callback = [=](int r, const char *k, size_t l) mutable {
                if (!r) {
                    buffer.append(k,l);
                }
                if (!r && http_body_is_final(&req->parser)) {
                    ref.push();
                    wrap_object(req, L);
                    lua_pushboolean(L, r == 0);
                    lua_pushlstring(L, buffer.data(), buffer.size());
                    if (lua_pcall(L, 3, 0, 0)) {
                        printf("http request callback: %s\n", lua_tostring(L, -1));
                    }
                }
            };
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
