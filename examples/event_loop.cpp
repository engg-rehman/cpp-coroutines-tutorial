#include <coroutine>
#include <deque>
#include <exception>
#include <iostream>
#include <utility>

class event_loop {
public:
    struct schedule_awaiter {
        event_loop& loop;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) const {
            loop.ready_.push_back(handle);
        }
        void await_resume() const noexcept {}
    };

    schedule_awaiter schedule() noexcept { return {*this}; }

    void run() {
        while (!ready_.empty()) {
            auto handle = ready_.front();
            ready_.pop_front();
            if (!handle.done()) {
                handle.resume();
            }
        }
    }

private:
    std::deque<std::coroutine_handle<>> ready_;
};

class job {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        job get_return_object() noexcept {
            return job{handle_type::from_promise(*this)};
        }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { std::terminate(); }
    };

    explicit job(handle_type handle) noexcept : handle_(handle) {}
    job(const job&) = delete;
    job& operator=(const job&) = delete;
    job(job&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    ~job() {
        if (handle_) {
            handle_.destroy();
        }
    }

private:
    handle_type handle_;
};

job example(event_loop& loop) {
    std::cout << "A\n";
    co_await loop.schedule();
    std::cout << "B\n";
    co_await loop.schedule();
    std::cout << "C\n";
}

int main() {
    event_loop loop;
    auto work = example(loop); // work must outlive queued non-owning handles
    std::cout << "main\n";
    loop.run();
}
