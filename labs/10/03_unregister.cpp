#include "../09/task.hpp"
#include "event_loop.hpp"
#include <iostream>
task<void> work(event_loop& loop) {
    struct guard { ~guard() { std::cout << "cleanup\n"; } } local;
    co_await loop.schedule();
    std::cout << "finished\n";
}
int main() {
    event_loop loop;
    { auto owner = work(loop); owner.start(); }
    std::cout << "queue empty: " << loop.empty() << '\n';
    loop.run();
    auto completed = work(loop); completed.start(); loop.run(); completed.result();
}
