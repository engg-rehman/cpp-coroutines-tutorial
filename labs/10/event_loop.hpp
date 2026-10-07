#pragma once
#include <algorithm>
#include <coroutine>
#include <deque>
class event_loop {
    std::deque<std::coroutine_handle<>> ready_;
    void remove(std::coroutine_handle<> h) noexcept { std::erase(ready_, h); }
public:
    class schedule_awaiter {
        event_loop& loop_;
        std::coroutine_handle<> queued_{};
    public:
        explicit schedule_awaiter(event_loop& loop) noexcept : loop_(loop) {}
        schedule_awaiter(const schedule_awaiter&) = delete;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            queued_ = h;
            loop_.ready_.push_back(h);
        }
        void await_resume() noexcept { queued_ = {}; }
        ~schedule_awaiter() { if (queued_) loop_.remove(queued_); }
    };
    schedule_awaiter schedule() { return schedule_awaiter{*this}; }
    bool run_one() {
        if (ready_.empty()) return false;
        auto h = ready_.front(); ready_.pop_front(); h.resume(); return true;
    }
    void run() { while (run_one()) {} }
    bool empty() const noexcept { return ready_.empty(); }
};
