#include "../02/manual_job.hpp"
#include <iostream>
manual_job work() { std::cout << "body\n"; co_return; }
int main() {
    auto owner = work();
    auto typed = owner.native_handle();
    std::coroutine_handle<> erased = typed;
    auto roundtrip = std::coroutine_handle<>::from_address(erased.address());
    std::cout << "same state: " << (roundtrip == erased) << '\n';
    auto moved = std::move(owner);
    roundtrip.resume();
    std::cout << "done: " << moved.done() << '\n';
    std::noop_coroutine().resume();
}
