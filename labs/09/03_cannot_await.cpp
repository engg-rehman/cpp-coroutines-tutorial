#include "result_task.hpp"
task<int> answer() { co_return 42; }
task<int> parent() { co_return (co_await answer()) + 1; }
int main() { auto work = parent(); return work.run(); }
