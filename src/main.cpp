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

static int mod_filter(const struct dirent *dir)
{
    if (dir->d_name[0] == '.') {
        return 0;
    }
    return 1;
}

#ifndef TEST
int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    uv_loop_t loop;
    uv_loop_init(&loop);

    EventBus bus;
    vector<unique_ptr<LuaModule>> modules;
    struct dirent **namelist;
    int n = scandir("modules", &namelist, mod_filter, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }
    for (unsigned i = 0; i < (unsigned)n; i++) {
        const char *dname = namelist[i]->d_name;
        const char *end = strrchr(dname, '.');
        string stripped = end? string(dname, end - dname) : string(dname);
        modules.emplace_back(new LuaModule(stripped.c_str()));
        auto &mod = *modules.back();
        bus.addBus(mod.bus);
        mod.loadrun(&loop);
    }

    string name = "Opal";
    string addr = "irc.stormbit.net";
    vector<string> chans = {
        "#test"
    };
    IrcServer irc(addr, name, name, name, move(chans), bus);
    if (argc < 2) {
        irc.start(addr.c_str(), "6667", &loop);
    }

    uv_run(&loop, UV_RUN_DEFAULT);

    return 0;
}
#endif
