#include "Util.h"

using namespace std;

string std::to_string(struct addrinfo *addr)
{
    char buf[4096];
    void *a = NULL;
    unsigned short port = 0;
    struct sockaddr_in *sa;
    struct sockaddr_in6 *sa6;
    switch (addr->ai_family) {
    case AF_INET:
        sa = reinterpret_cast<struct sockaddr_in*>(addr->ai_addr);
        a = &sa->sin_addr;
        port = sa->sin_port;
        break;
    case AF_INET6:
        sa6 = reinterpret_cast<struct sockaddr_in6*>(addr->ai_addr);
        a = &sa6->sin6_addr;
        port = sa6->sin6_port;
        break;
    default:
        break;
    }
    return string(inet_ntop(addr->ai_family, a, buf, 4096)) + ":" + to_string(ntohs(port));
}

static void walk_cb(uv_handle_t *handle, void *arg)
{
    (void)arg;
#define XX(t, n) #n,
    static const char *const names[] = {
        UV_HANDLE_TYPE_MAP(XX)
        "file",
        NULL
    };
#undef XX
    printf("type: %s; ", handle->type < UV_HANDLE_TYPE_MAX? names[handle->type] : "Out of range");
    printf("active: %u; ", uv_is_active(handle));
    printf("closing: %u; ", uv_is_closing(handle));
    printf("ref: %u\n", uv_has_ref(handle));
}

void bot::print_loop(uv_loop_t *loop)
{
    uv_walk(loop, &walk_cb, NULL);
}
