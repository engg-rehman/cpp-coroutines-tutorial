#include "../02/manual_job.hpp"
#include "event_loop.hpp"
#include <iostream>
manual_job work(event_loop& loop) {
    std::cout << "A\n";
    co_await loop.schedule();
    std::cout << "B\n";
}
int main() {
    event_loop loop;
    auto owner = work(loop);
    owner.resume();
    std::cout << "main\n";
    loop.run();
    std::cout << "done: " << owner.done() << "\n";
}
