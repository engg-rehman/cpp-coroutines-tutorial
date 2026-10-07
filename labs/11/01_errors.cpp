#include "../09/task.hpp"
#include <iostream>
task<int> child() { throw std::runtime_error("read failed"); co_return 0; }
task<int> parent() {
    try { co_return co_await child(); }
    catch (const std::runtime_error& e) { std::cout << "parent caught: " << e.what() << '\n'; co_return -1; }
}
int main() { auto work = parent(); int value = work.run(); std::cout << "result: " << value << '\n'; }
