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

static void sigint_callback(uv_signal_t *handle, int signum)
{
    (void)signum;
    uv_stop(handle->loop);
}

#ifndef TEST
int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    uv_loop_t loop;
    uv_loop_init(&loop);

    uv_signal_t sigint;
    uv_signal_init(&loop, &sigint);
    uv_signal_start(&sigint, &sigint_callback, SIGINT);

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

    YAML::Node config = YAML::LoadFile("config.yaml");
    vector<unique_ptr<IrcServer>> servers;

    for (auto n : config["servers"]) {
        string name = n["nick"].IsDefined()? n["nick"].as<string>() : "Opal";
        string addr = n["address"].as<string>();
        string port = n["port"].IsDefined()? n["port"].as<string>() : "6667";
        vector<string> chans;
        for (auto c : n["channels"]) {
            chans.emplace_back(c.as<string>());
        }
        servers.emplace_back(new IrcServer(addr, name, name, name, move(chans), bus));
        servers.back()->start(addr.c_str(), port.c_str(), &loop);
    }

    uv_run(&loop, UV_RUN_DEFAULT);

    return 0;
}
#endif
