#pragma once
#include <coroutine>
#include <exception>
#include <utility>
#include <iostream>
class manual_job {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    struct promise_type {
        promise_type() { std::cout << "promise: construct\n"; }
        ~promise_type() { std::cout << "promise: destroy\n"; }
        manual_job get_return_object() noexcept {
            std::cout << "promise: return object\n";
            return manual_job{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { std::cout << "promise: initial_suspend\n"; return {}; }
        std::suspend_always final_suspend() const noexcept { std::cout << "promise: final_suspend\n"; return {}; }
        void return_void() const noexcept { std::cout << "promise: return_void\n";}
        void unhandled_exception() const noexcept { std::terminate(); }
    };
    explicit manual_job(handle_type h) noexcept : handle_(h) {}
    manual_job(const manual_job&) = delete;
    manual_job& operator=(const manual_job&) = delete;
    manual_job(manual_job&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}
    ~manual_job() { if (handle_) { std::cout << "owner: destroy frame\n"; handle_.destroy(); } }
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
