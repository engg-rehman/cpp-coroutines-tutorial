#include "task.hpp"

#include <iostream>
#include <stdexcept>

task<int> fails() {
    throw std::runtime_error("operation failed");
    co_return 0;
}

task<int> handles_failure() {
    try {
        co_return co_await fails();
    } catch (const std::runtime_error& error) {
        std::cout << "caught: " << error.what() << '\n';
        co_return -1;
    }
}

int main() {
    auto result = handles_failure();
    const int value = result.run();
    std::cout << "result: " << value << '\n';
}
