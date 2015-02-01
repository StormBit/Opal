#ifndef BOT_PROMISE_H
#define BOT_PROMISE_H

#include <vector>
#include <functional>
#include <memory>

namespace bot {

template<typename T>
class Promise {
public:
    Promise() {}
    Promise(Promise&&) = delete;

    void then(std::function<void (T)> &&cb) {
        callbacks_.push_back(std::move(cb));
    }

    template<typename U, typename F>
    Promise<U> &then(F &&cb) {
        Promise<U> *p = new Promise<U>;
        callbacks_.push_back([=](T val) {
                auto res = cb(val);
                p->run(res);
                delete p;
            });
        return *p;
    }

    void run(T val) {
        for (auto &cb : callbacks_) {
            cb(val);
        }
    }

private:
    std::vector<std::function<void (T)>> callbacks_;
};

template<>
class Promise<void> {
public:
    Promise() {}
    Promise(Promise&&) = delete;

    void then(std::function<void ()> &&cb) {
        callbacks_.push_back(std::move(cb));
    }

    template<typename U, typename F>
    Promise<U> &then(F &&cb) {
        Promise<U> *p = new Promise<U>;
        callbacks_.push_back([=]() {
                auto res = cb();
                p->run(res);
                delete p;
            });
        return *p;
    }

    void run() {
        for (auto &cb : callbacks_) {
            cb();
        }
    }

private:
    std::vector<std::function<void ()>> callbacks_;
};

}

#endif
