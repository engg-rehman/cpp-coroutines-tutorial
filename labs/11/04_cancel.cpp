#include "../09/task.hpp"
#include "../10/event_loop.hpp"
#include "cancel.hpp"
#include <iostream>
task<int> work(event_loop& loop, std::stop_token token) {
    struct guard { ~guard() { std::cout << "cleanup\n"; } } local;
    for (int n = 1; n <= 3; ++n) {
        check_stop(token);
        co_await loop.schedule();
        check_stop(token);
        std::cout << "item " << n << '\n';
    }
    co_return 3;
}
int main() {
    event_loop loop; std::stop_source stop;
    auto owner = work(loop, stop.get_token()); owner.start();
    loop.run_one();
    stop.request_stop();
    std::cout << "stop requested\n";
    loop.run();
    try { owner.result(); } catch (const cancelled& e) { std::cout << e.what() << '\n'; }
    std::cout << "queue empty: " << loop.empty() << '\n';
}
