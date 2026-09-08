#include "task.hpp"

#include <iostream>

task<int> answer() {
    co_return 42;
}

task<int> add_one() {
    const int value = co_await answer();
    co_return value + 1;
}

int main() {
    auto result = add_one();
    std::cout << result.run() << '\n';
}
