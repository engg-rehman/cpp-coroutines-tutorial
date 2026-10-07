#include "../02/manual_job.hpp"
#include <iostream>
#include <string>
manual_job print_later(std::string text) { std::cout << text << '\n'; co_return; }
int main() {
    auto work = [](std::string text) -> manual_job {
        std::cout << text << '\n'; co_return;
    }("parameter survives");
    work.resume();
    auto named = print_later("named function survives");
    named.resume();
}
