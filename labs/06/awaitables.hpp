#pragma once
#include <coroutine>
#include <iostream>
struct immediate {
    int value;
    bool await_ready() const noexcept { std::cout << "ready\n"; return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    int await_resume() const noexcept { std::cout << "result\n"; return value; }
};
struct request {
    int value;
    immediate operator co_await() const noexcept {
        std::cout << "member conversion\n"; return {value};
    }
};
struct free_request { int value; };
inline immediate operator co_await(free_request r) noexcept {
    std::cout << "free conversion\n"; return {r.value};
}
