#include "../09/task.hpp"
task<void> cleanup() { co_return; }
task<void> work() {
    try { throw 1; }
    catch (...) { co_await cleanup(); }
}
int main() { auto owner = work(); owner.run(); }
