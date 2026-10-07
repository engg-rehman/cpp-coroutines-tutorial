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
    work.resume();
    std::cout << "main: other work\n";
    work.resume();
    std::cout << "done: " << work.done() << "\n";
}
