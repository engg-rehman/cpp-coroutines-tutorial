#include "../02/manual_job.hpp"
#include <iostream>
struct pause {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() const noexcept {}
};
manual_job work() { co_await pause{}; std::cout << "after\n"; }
int main() { auto owner = work(); owner.resume(); owner.resume(); }
