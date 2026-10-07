#include "task.hpp"
#include <iostream>
task<int> answer() { std::cout << "child\n"; co_return 42; }
task<int> parent() { int n = co_await answer(); std::cout << "parent\n"; co_return n + 1; }
task<void> notify() { std::cout << "void task\n"; co_return; }
int main() {
    auto work = parent(); int result = work.run(); std::cout << result << '\n';
    auto signal = notify(); signal.run();
    try { work.run(); } catch (const std::logic_error& e) { std::cout << e.what() << '\n'; }
}
