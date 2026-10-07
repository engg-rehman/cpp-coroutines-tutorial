#include "../02/manual_job.hpp"
#include "awaitables.hpp"
manual_job work() {
    int n = co_await request{42};
    std::cout << n << '\n';
}
int main() { auto owner = work(); owner.resume(); }
