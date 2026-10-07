#include <coroutine>
void hello() { co_return; }
int main() { hello(); }
