#include "../02/manual_job.hpp"
#include <iostream>
#include <string>
manual_job print_later(const std::string& text) {
    std::cout << text << '\n';
    co_return;
}
int main() {
    std::string text = "original";
    auto work = print_later(text);
    text = "changed";
    work.resume();
}
