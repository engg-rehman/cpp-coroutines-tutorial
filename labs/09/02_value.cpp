#include "result_task.hpp"
#include <iostream>
task<int> answer() { co_return 42; }
int main() { auto work = answer(); std::cout << work.run() << '\n'; }
