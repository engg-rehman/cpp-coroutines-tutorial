#include "../09/task.hpp"
#include <iostream>
task<void> cleanup() { std::cout << "cleanup awaited\n"; co_return; }
task<void> work() {
    std::exception_ptr error;
    try { throw std::runtime_error("read failed"); }
    catch (...) { error = std::current_exception(); }
    co_await cleanup();
    if (error) std::rethrow_exception(error);
}
int main() {
    auto owner = work();
    try { owner.run(); } catch (const std::exception& e) { std::cout << e.what() << '\n'; }
}
