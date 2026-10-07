#include <coroutine>
#include <exception>
#include <iostream>
struct job {
    struct promise_type {
        job get_return_object() noexcept { return {}; }
    };
};
job hello() { std::cout << "hello\n"; co_return; }
int main() { hello(); }
