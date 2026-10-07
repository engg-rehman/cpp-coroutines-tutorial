#include "unlinked_task.hpp"
#include <iostream>
task<int> answer() { std::cout << "child finished\n"; co_return 42; }
task<int> parent() { co_return (co_await answer()) + 1; }
int main() {
    auto work = parent();
    try { std::cout << work.run() << '\n'; }
    catch (const std::logic_error& e) { std::cout << e.what() << '\n'; }
}
