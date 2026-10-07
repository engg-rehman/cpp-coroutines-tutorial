#include "../02/manual_job.hpp"
#include "receive.hpp"
#include <iostream>
manual_job work(fake_api& api) {
    int value = co_await receive{api};
    std::cout << "value: " << value << '\n';
}
int main() {
    for (bool inline_completion : {true, false}) {
        fake_api api{inline_completion, {}};
        auto owner = work(api);
        owner.resume();
        std::cout << "back in main\n";
        api.complete(); api.complete();
    }
    fake_api late;
    { auto abandoned = work(late); abandoned.resume(); }
    late.complete();
    std::cout << "late callback ignored\n";
}
