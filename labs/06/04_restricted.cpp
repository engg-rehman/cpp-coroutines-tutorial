#include "restricted_job.hpp"
#include "awaitables.hpp"
manual_job work() {
    int n = co_await std::suspend_never{};
    std::cout << n << '\n';
}
int main() { auto owner = work(); owner.resume(); }
