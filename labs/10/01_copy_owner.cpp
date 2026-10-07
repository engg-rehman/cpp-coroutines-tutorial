#include "../02/manual_job.hpp"
manual_job work() { co_return; }
int main() { auto first = work(); auto second = first; }
