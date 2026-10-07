#pragma once
#include <coroutine>
#include <deque>
class event_loop {
    std::deque<std::coroutine_handle<>> ready_;
public:
    struct schedule_awaiter {
        event_loop& loop;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) { loop.ready_.push_back(h); }
        void await_resume() const noexcept {}
    };
    schedule_awaiter schedule() { return {*this}; }
    bool run_one() {
        if (ready_.empty()) return false;
        auto h = ready_.front();
        ready_.pop_front(); // Remove before resuming: resume may enqueue again.
        h.resume();
        return true;
    }
    void run() { while (run_one()) {} }
};
