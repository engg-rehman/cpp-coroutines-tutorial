#include "policy_job.hpp"
#include "awaitables.hpp"
manual_job work() {
    int n = co_await free_request{7};
    std::cout << n << '\n';
}
int main() { auto owner = work(); owner.resume(); }
