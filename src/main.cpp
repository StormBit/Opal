#include <uv.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <deque>
#include <cstring>

#include "IrcParser.h"
#include "LuaModule.h"
#include "EventBus.h"
#include "IrcServer.h"
#include "DnsResolver.h"
#include "TcpConnection.h"
#include "HttpRequest.h"

using namespace std;
using namespace bot;

void print(const string &str)
{
    printf("%s\n", str.c_str());
}

#ifndef TEST
int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    uv_loop_t loop;
    uv_loop_init(&loop);

    EventBus bus;
    LuaModule test("test");
    test.load(&loop);
    test.openlib(bus);
    HttpRequest::openlib(test.getL());
    test.run();

    string name = "Opal";
    string addr = "irc.stormbit.net";
    vector<string> chans = {
        "#test"
    };
    IrcServer irc(addr, name, name, name, move(chans), bus);
    irc.start(addr.c_str(), "6667", &loop);

    uv_run(&loop, UV_RUN_DEFAULT);

    return 0;
}
#endif
