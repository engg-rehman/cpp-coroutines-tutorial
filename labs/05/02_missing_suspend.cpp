#include "../02/manual_job.hpp"
#include <iostream>
struct pause {
    bool await_ready() const noexcept { return false; }
};
manual_job work() { co_await pause{}; std::cout << "after\n"; }
int main() { auto owner = work(); owner.resume(); owner.resume(); }
