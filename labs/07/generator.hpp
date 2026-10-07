#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>
template<class T> class generator {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    struct promise_type {
        std::optional<T> current;
        std::exception_ptr error;
        generator get_return_object() noexcept { return generator{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        std::suspend_always yield_value(T value) {
            current.emplace(std::move(value)); return {};
        }
        void return_void() const noexcept {}
        void unhandled_exception() noexcept { error = std::current_exception(); }
        // A synchronous next() cannot drive external async operations.
        template<class A> void await_transform(A&&) = delete;
    };
    explicit generator(handle_type h) noexcept : handle_(h) {}
    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;
    generator(generator&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    ~generator() { if (handle_) handle_.destroy(); }
    bool next() {
        if (!handle_ || handle_.done()) return false;
        handle_.promise().current.reset();
        handle_.resume();
        if (handle_.promise().error) std::rethrow_exception(handle_.promise().error);
        return !handle_.done();
    }
    const T& value() const {
        if (!handle_ || handle_.done() || !handle_.promise().current)
            throw std::logic_error("call next() successfully before value()");
        return *handle_.promise().current;
    }
private:
    handle_type handle_;
};
