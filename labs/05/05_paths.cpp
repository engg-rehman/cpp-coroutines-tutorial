#include "../02/manual_job.hpp"
#include <iostream>
struct value_awaiter {
    bool ready;
    bool stay_suspended;
    bool await_ready() const noexcept { std::cout << "ready\n"; return ready; }
    bool await_suspend(std::coroutine_handle<>) const noexcept {
        std::cout << "suspend\n"; return stay_suspended;
    }
    int await_resume() const noexcept { std::cout << "resume\n"; return 42; }
};
manual_job work(bool ready, bool stay) {
    int result = co_await value_awaiter{ready, stay};
    std::cout << "value: " << result << '\n';
}
int main() {
    for (auto flags : {std::pair{true, false}, std::pair{false, false}, std::pair{false, true}}) {
        std::cout << "start\n";
        auto owner = work(flags.first, flags.second);
        owner.resume();
        std::cout << "back in main\n";
        owner.resume();
    }
}
