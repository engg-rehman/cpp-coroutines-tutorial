#pragma once
#include <coroutine>
#include <functional>
#include <memory>
#include <utility>
// Deliberately single-threaded. start() either calls inline or stores one callback.
struct fake_api {
    bool inline_completion = false;
    std::function<void(int)> pending;
    void start(std::function<void(int)> callback) {
        if (inline_completion) callback(42);
        else pending = std::move(callback);
    }
    void complete(int value = 42) {
        if (pending) { auto callback = pending; callback(value); }
    }
};
struct receive {
    struct state {
        bool completed = false;
        int value = 0;
        std::coroutine_handle<> continuation{};
    };
    fake_api& api;
    std::shared_ptr<state> operation = std::make_shared<state>();
    bool await_ready() const noexcept { return operation->completed; }
    bool await_suspend(std::coroutine_handle<> h) {
        auto shared = operation;
        api.start([shared](int value) {
            if (shared->completed) return; // Ignore duplicate completion.
            shared->value = value;
            shared->completed = true;
            auto next = std::exchange(shared->continuation, {});
            if (next) next.resume(); // No frame/awaiter access after publication.
        });
        if (shared->completed) return false; // Inline completion: no external resume.
        shared->continuation = h;
        return true;
    }
    int await_resume() const noexcept { return operation->value; }
    ~receive() { operation->continuation = {}; } // Makes late callbacks harmless here.
};
