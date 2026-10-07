#pragma once
#include <coroutine>
#include <exception>
#include <utility>
class manual_job {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    struct promise_type {
        manual_job get_return_object() noexcept {
            return manual_job{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { std::terminate(); }
    };
    explicit manual_job(handle_type h) noexcept : handle_(h) {}
    manual_job(const manual_job&) = delete;
    manual_job& operator=(const manual_job&) = delete;
    manual_job(manual_job&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}
    ~manual_job() { if (handle_) handle_.destroy(); }
    bool resume() {
        if (!handle_ || handle_.done()) return false;
        handle_.resume();
        return !handle_.done();
    }
    bool done() const noexcept { return !handle_ || handle_.done(); }
    handle_type native_handle() const noexcept { return handle_; }
private:
    handle_type handle_;
};
