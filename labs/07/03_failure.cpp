#include "generator.hpp"
#include <iostream>
generator<int> numbers() { co_yield 1; throw std::runtime_error("producer failed"); }
int main() {
    auto values = numbers();
    try { while (values.next()) std::cout << values.value() << '\n'; }
    catch (const std::exception& e) { std::cout << e.what() << '\n'; }
}
