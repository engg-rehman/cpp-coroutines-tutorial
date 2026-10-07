#include "manual_job.hpp"
#include <iostream>
manual_job count(char name) {
    for (int n = 1; n <= 2; ++n) {
        std::cout << name << n << '\n';
        co_await std::suspend_always{};
    }
}
int main() {
    auto a = count('A'); auto b = count('B');
    while (!a.done() || !b.done()) { a.resume(); b.resume(); }
}
