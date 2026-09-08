#include <coroutine>
#include <exception>
#include <iostream>
#include <utility>

class manual_task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        manual_task get_return_object() noexcept {
            return manual_task{handle_type::from_promise(*this)};
        }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { std::terminate(); }
    };

    explicit manual_task(handle_type handle) noexcept : handle_(handle) {}
    manual_task(const manual_task&) = delete;
    manual_task& operator=(const manual_task&) = delete;

    manual_task(manual_task&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    manual_task& operator=(manual_task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    ~manual_task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    bool resume() {
        if (!handle_ || handle_.done()) {
            return false;
        }
        handle_.resume();
        return !handle_.done();
    }

private:
    handle_type handle_;
};

manual_task demonstration() {
    std::cout << "coroutine: start\n";
    co_await std::suspend_always{};
    std::cout << "coroutine: resumed\n";
}

int main() {
    auto task = demonstration();
    std::cout << "main: before resume\n";
    task.resume();
    std::cout << "main: after resume\n";
}
