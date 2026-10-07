#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>
// Intermediate checkpoint: owns and returns a result, but cannot be co_awaited.
template<class T> class task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    struct promise_type {
        std::optional<T> value;
        std::exception_ptr error;
        task get_return_object() noexcept { return task{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        template<class U> void return_value(U&& result) {
            value.emplace(std::forward<U>(result));
        }
        void unhandled_exception() noexcept { error = std::current_exception(); }
    };
    explicit task(handle_type h) noexcept : handle_(h) {}
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& other) noexcept
        : handle_(std::exchange(other.handle_, {})), started_(other.started_) {}
    ~task() { if (handle_) handle_.destroy(); }
    T run() {
        if (!handle_) throw std::logic_error("empty task");
        if (started_) throw std::logic_error("task already started");
        started_ = true;
        handle_.resume();
        if (!handle_.done()) throw std::logic_error("task needs an external scheduler");
        auto& p = handle_.promise();
        if (p.error) std::rethrow_exception(p.error);
        if (!p.value) throw std::logic_error("missing result");
        return std::move(*p.value);
    }
private:
    handle_type handle_;
    bool started_ = false;
};
