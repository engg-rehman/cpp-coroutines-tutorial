#include "../07/generator.hpp"
#include "../09/task.hpp"
#include "../10/event_loop.hpp"
#include "../11/cancel.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

generator<std::string> lines(std::string path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open input");
    std::string line;
    while (std::getline(input, line)) co_yield line;
    if (input.bad()) throw std::runtime_error("input read failed");
}
task<int> parse_reading(event_loop& loop, std::stop_token token,
                        std::string text, int id) {
    check_stop(token);
    std::cout << "start " << id << '\n';
    co_await loop.schedule(); // Simulated service wait, not asynchronous file I/O.
    check_stop(token);
    int value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 0 || value > 100)
        throw std::runtime_error("invalid reading");
    std::cout << "finish " << id << '\n';
    co_return value;
}
task<int> process(event_loop& loop, std::stop_token token, std::string text, int id) {
    int value = co_await parse_reading(loop, token, std::move(text), id);
    co_return value;
}
int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: report FILE [WIDTH 1..8] [cancel]\n"; return 2;
    }
    try {
        int width = 2;
        if (argc >= 3) {
            std::string arg = argv[2];
            auto [end, error] = std::from_chars(arg.data(), arg.data() + arg.size(), width);
            if (error != std::errc{} || end != arg.data() + arg.size() || width < 1 || width > 8)
                throw std::runtime_error("width must be 1..8");
        }
        if (argc == 4 && std::string(argv[3]) != "cancel") throw std::runtime_error("unknown mode");
        bool cancel_first_batch = argc == 4;
        event_loop loop; std::stop_source stop;
        auto source = lines(argv[1]);
        int accepted = 0, rejected = 0, stopped = 0, id = 0;
        long long total = 0;
        for (;;) {
            std::vector<task<int>> batch;
            for (int i = 0; i < width && source.next(); ++i)
                batch.push_back(process(loop, stop.get_token(), source.value(), ++id));
            if (batch.empty()) break;
            for (auto& job : batch) job.start();
            if (cancel_first_batch) stop.request_stop();
            loop.run(); // Join every started child before observing errors or leaving the scope.
            for (auto& job : batch) {
                try { total += job.result(); ++accepted; }
                catch (const cancelled&) { ++stopped; }
                catch (const std::runtime_error& e) { ++rejected; std::cout << "reject: " << e.what() << '\n'; }
            }
            if (stop.stop_requested()) break;
        }
        std::cout << "accepted=" << accepted << " rejected=" << rejected
                  << " cancelled=" << stopped << " total=" << total << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
