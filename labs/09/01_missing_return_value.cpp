#include "../02/manual_job.hpp"
manual_job answer() { co_return 42; }
int main() { auto work = answer(); work.resume(); }
