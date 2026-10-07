#include <iostream>
void count() { int n = 0; std::cout << ++n << '\n'; }
int main() { count(); count(); }
