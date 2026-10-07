#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
// Only the appropriate return hook is inherited by each promise specialization.
template<class T> struct result_storage {
    std::optional<T> value;
    template<class U> void return_value(U&& v) { value.emplace(std::forward<U>(v)); }
    T take() {
        if (!value) throw std::logic_error("missing result");
        return std::move(*value);
    }
};
template<> struct result_storage<void> {
    void return_void() const noexcept {}
    void take() const noexcept {}
};
template<class T> class task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    struct promise_type : result_storage<T> {
        std::exception_ptr error;
        std::coroutine_handle<> continuation = std::noop_coroutine();
        task get_return_object() noexcept { return task{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) const noexcept {
                return h.promise().continuation;
            }
            void await_resume() const noexcept {}
        };
        final_awaiter final_suspend() const noexcept { return {}; }
        void unhandled_exception() noexcept { error = std::current_exception(); }
    };
    explicit task(handle_type h) noexcept : handle_(h) {}
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& other) noexcept
        : handle_(std::exchange(other.handle_, {})), started_(other.started_), consumed_(other.consumed_) {}
    ~task() { if (handle_) handle_.destroy(); }
    struct awaiter {
        handle_type handle;
        bool await_ready() const noexcept { return handle.done(); }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) const noexcept {
            (void)parent; // BUG: the parent continuation is lost.
            return handle;
        }
        T await_resume() const {
            auto& p = handle.promise();
            if (p.error) std::rethrow_exception(p.error);
            return p.take();
        }
    };
    // Borrow the handle. The task temporary/named owner must live through the await.
    awaiter operator co_await() && { claim_start(); consumed_ = true; return {handle_}; }
    awaiter operator co_await() & = delete;
    void start() { claim_start(); handle_.resume(); }
    bool done() const noexcept { return handle_ && handle_.done(); }
    T result() {
        if (!done()) throw std::logic_error("task is not complete");
        if (consumed_) throw std::logic_error("result already consumed");
        consumed_ = true;
        return awaiter{handle_}.await_resume();
    }
    T run() {
        start();
        if (!done()) throw std::logic_error("task needs an external scheduler");
        return result();
    }
private:
    void claim_start() {
        if (!handle_) throw std::logic_error("empty task");
        if (started_) throw std::logic_error("task already started");
        started_ = true;
    }
    handle_type handle_;
    bool started_ = false;
    bool consumed_ = false;
};
