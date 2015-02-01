#ifndef BOT_TIMER_H
#define BOT_TIMER_H

#include <uv.h>

#include "Promise.h"

namespace bot {

class Timer {
public:
    Timer() = default;
    Timer(Timer &) = delete;
    Timer(Timer &&) = delete;

    void start(uv_loop_t *loop, uint64_t timeout, uint64_t repeat = 0);
    void stop();
    void again();
    void close(std::function<void ()> &&cb);

    Promise<void> promise;

private:
    static void timer_cb(uv_timer_t *handle);
    static void close_cb(uv_handle_t *handle);

    std::function<void ()> close_fn;
    uv_timer_t timer;
};

}

#endif
