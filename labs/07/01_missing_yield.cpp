#include "../02/manual_job.hpp"
manual_job numbers() { co_yield 1; }
int main() { auto owner = numbers(); owner.resume(); }
