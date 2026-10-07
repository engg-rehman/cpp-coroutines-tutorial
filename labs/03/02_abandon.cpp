#include "traced_job.hpp"
struct guard {
    guard() { std::cout << "local: construct\n"; }
    ~guard() { std::cout << "local: destroy\n"; }
};
manual_job work() {
    guard local;
    std::cout << "body: first\n";
    co_await std::suspend_always{};
    std::cout << "body: last\n";
    co_return;
}
int main() {
    std::cout << "main: call\n";
    auto owner = work();
    std::cout << "main: resume 1\n";
    owner.resume();

    std::cout << "main: leave\n";
}
