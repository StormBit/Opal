#include "Timer.h"

using namespace bot;
using namespace std;

void Timer::start(uv_loop_t *loop, uint64_t timeout, uint64_t repeat)
{
    uv_timer_init(loop, &timer);
    timer.data = this;
    uv_timer_start(&timer, &Timer::timer_cb, timeout, repeat);
}

void Timer::stop()
{
    uv_timer_stop(&timer);
}

void Timer::again()
{
    uv_timer_again(&timer);
}

void Timer::close(function<void ()> &&cb)
{
    close_fn = move(cb);
    uv_close(reinterpret_cast<uv_handle_t*>(&timer), &Timer::close_cb);
}

void Timer::timer_cb(uv_timer_t *handle)
{
    Timer &self = *reinterpret_cast<Timer*>(handle->data);
    self.promise.run();
}

void Timer::close_cb(uv_handle_t *handle)
{
    Timer &self = *reinterpret_cast<Timer*>(handle->data);
    self.close_fn();
}
