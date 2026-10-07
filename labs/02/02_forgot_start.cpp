#include "manual_job.hpp"
#include <iostream>
manual_job count() {
    int n = 0;
    std::cout << ++n << '\n';
    co_await std::suspend_always{};
    std::cout << ++n << '\n';
}
int main() {
    auto work = count();
    std::cout << "main finished\n";
}
