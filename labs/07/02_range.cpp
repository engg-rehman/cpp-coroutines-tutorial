#include "generator.hpp"
#include <iostream>
generator<int> numbers() {
    for (int n = 1; n <= 3; ++n) co_yield n;
}
int main() {
    auto values = numbers();
    while (values.next()) std::cout << values.value() << '\n';
    std::cout << "done: " << !values.next() << '\n';
}
