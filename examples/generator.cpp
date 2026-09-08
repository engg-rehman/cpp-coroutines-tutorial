#include <coroutine>
#include <exception>
#include <iostream>
#include <optional>
#include <utility>

template <class T>
class generator {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T> current;
        std::exception_ptr exception;

        generator get_return_object() noexcept {
            return generator{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        std::suspend_always yield_value(T value) noexcept {
            current.emplace(std::move(value));
            return {};
        }
        void return_void() const noexcept {}
        void unhandled_exception() noexcept { exception = std::current_exception(); }
    };

    explicit generator(handle_type handle) noexcept : handle_(handle) {}
    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;
    generator(generator&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}
    ~generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    bool next() {
        if (!handle_ || handle_.done()) {
            return false;
        }
        handle_.resume();
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return !handle_.done();
    }

    const T& value() const noexcept { return *handle_.promise().current; }

private:
    handle_type handle_;
};

generator<int> fibonacci() {
    int a = 0;
    int b = 1;
    while (true) {
        co_yield a;
        const int next = a + b;
        a = b;
        b = next;
    }
}

int main() {
    auto values = fibonacci();
    for (int i = 0; i < 10 && values.next(); ++i) {
        std::cout << values.value() << (i == 9 ? '\n' : ' ');
    }
}
