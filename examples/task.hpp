#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

template <class T>
class task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation = std::noop_coroutine();

        task get_return_object() noexcept {
            return task{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }

        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type current) const noexcept {
                return current.promise().continuation;
            }
            void await_resume() const noexcept {}
        };

        final_awaiter final_suspend() const noexcept { return {}; }

        template <class U>
        void return_value(U&& result) {
            value.emplace(std::forward<U>(result));
        }

        void unhandled_exception() noexcept { exception = std::current_exception(); }
    };

    explicit task(handle_type handle) noexcept : handle_(handle) {}
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    ~task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    struct awaiter {
        handle_type handle;

        bool await_ready() const noexcept { return !handle || handle.done(); }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> continuation) const noexcept {
            handle.promise().continuation = continuation;
            return handle;
        }

        T await_resume() const {
            if (!handle) {
                throw std::logic_error("awaited an empty task");
            }
            auto& promise = handle.promise();
            if (promise.exception) {
                std::rethrow_exception(promise.exception);
            }
            if (!promise.value) {
                throw std::logic_error("task has no result");
            }
            return std::move(*promise.value);
        }
    };

    awaiter operator co_await() const & noexcept { return {handle_}; }

    T run() {
        if (!handle_) {
            throw std::logic_error("empty task");
        }
        if (!handle_.done()) {
            handle_.resume();
        }
        if (!handle_.done()) {
            throw std::logic_error("task needs an external scheduler");
        }
        return awaiter{handle_}.await_resume();
    }

private:
    handle_type handle_;
};
