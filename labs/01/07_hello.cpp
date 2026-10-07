#include <coroutine>
#include <exception>
#include <iostream>
struct job {
    struct promise_type {
        job get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { std::terminate(); }
    };
};
job hello() { std::cout << "hello\n"; co_return; }
int main() { hello(); }
