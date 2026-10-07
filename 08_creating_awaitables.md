---
title: "Lesson 8: Build an Event Loop and Callback Adapter"
description: "Repair a stalled coroutine by scheduling its handle, then handle inline, deferred, duplicate, and late callback completion."
keywords:
  - custom C++ awaitable
  - create C++ awaiter
  - callback coroutine adapter
  - event loop coroutine
  - await_suspend race
seo:
  type: TechArticle
---

# Lesson 8: Make something else resume the coroutine

A real operation cannot require `main` to know every pause inside a function. We need an awaiter that arranges the next resume. First a ready queue, then an adapter for a callback API.

## 1. Observe a job waiting for a driver

[01_stalled.cpp](labs/08/01_stalled.cpp) starts a coroutine that prints `A`, awaits `loop.schedule()`, then prints `B`.

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/08/01_stalled.cpp -o scratch/loop
./scratch/loop
```

Expected:

```text
A
main
done before pumping: 0
B
```

At the diagnostic print, the job has not finished. The saved checkpoint then pumps the loop so cleanup remains valid. If you remove that pump, nothing automatically prints `B`: the coroutine language has not created a background event loop.

## 2. Implement the registration side

Open [event_loop.hpp](labs/08/event_loop.hpp). A `deque<coroutine_handle<>>` holds ready continuations. Its scheduling awaiter is:

```cpp
struct schedule_awaiter {
    event_loop& loop;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        loop.ready_.push_back(h);
    }
    void await_resume() const noexcept {}
};
```

The queue insertion happens after the coroutine is considered suspended. The handle is erased because the loop needs no promise-specific fields. `await_suspend` may throw if queue allocation fails; it is not marked `noexcept`.

The loop exposes `schedule()` returning this awaiter. The result is `void`: scheduling changes when we continue, not what value the expression yields.

## 3. Implement the driver side

Add:

```cpp
bool run_one() {
    if (ready_.empty()) return false;
    auto h = ready_.front();
    ready_.pop_front();
    h.resume();
    return true;
}
void run() { while (run_one()) {} }
```

Remove the entry **before** resuming. The resumed coroutine can enqueue itself again or finish and cause its awaiter to be destroyed.

Build [02_loop.cpp](labs/08/02_loop.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/08/02_loop.cpp -o scratch/loop
./scratch/loop
```

Expected:

```text
A
main
B
done: 1
```

Add a second `co_await loop.schedule()` and a `C` print in a local copy. `run()` should drain both scheduling turns. Replace `run()` with one `run_one()` to see exactly one turn, but drain the remaining entries before leaving scope.

This queue owns no frames. Keep each job owner alive until its entries are removed. In Lesson 10, we will implement automatic removal during abandonment. The loop itself must outlive all awaiters that refer to it.

## 4. Replace a queue with a callback API

A queue we own has predictable behavior. A callback API may complete immediately inside the call that registers it. Start with this tempting **incomplete design fragment**:

```cpp
void await_suspend(std::coroutine_handle<> h) {
    api.start([this, h](int value) {
        result = value;
        h.resume();
    });
    registered = true;
}
```

It assumes `this` will remain alive, permits duplicate resumes if the API calls twice, and touches the awaiter after `start` may have resumed and destroyed it. We will repair the protocol before executing it.

[receive.hpp](labs/08/receive.hpp) contains a fake API and a complete, deliberately **single-threaded** adapter. The fake API either invokes a callback inline or stores it until `complete()` is called. It owns one pending operation at a time. No network or worker thread is involved.

## 5. Move completion data into separately owned state

The adapter's state is:

```cpp
struct state {
    bool completed = false;
    int value = 0;
    std::coroutine_handle<> continuation{};
};
```

The awaiter and callback share this state using `shared_ptr`. This owns completion data; it does not own the coroutine frame. Before starting the API, `await_suspend` takes a local shared copy.

The callback ignores repeated completion, stores the value, marks completion, and takes the continuation out with `std::exchange`. If a continuation is present it resumes it, with no later access to the awaiter. Removing the handle before resumption prevents a duplicate callback from reusing it.

The key registration sequence is:

```cpp
api.start(callback);                 // May complete inline.
if (shared->completed) return false; // Continue without an external resume.
shared->continuation = h;
return true;                         // A later callback will resume us.
```

During inline completion there is no installed continuation, so the callback only records the result. `await_suspend(false)` then continues the await-expression once. During deferred completion, the callback finds the installed handle and resumes it once.

This closes the readiness/registration gap **under this fake API's single-thread contract**. A real concurrent completion between the `completed` check and handle assignment would break it. Do not use this adapter unchanged with a worker-thread callback.

## 6. Exercise inline, deferred, duplicate, and late completion

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/08/03_callback.cpp -o scratch/callback
./scratch/callback
```

[03_callback.cpp](labs/08/03_callback.cpp) produces:

```text
value: 42
back in main
back in main
value: 42
late callback ignored
```

The first operation completes inline: the value prints before returning to `main`. The second resumes only after `complete()`. Calling `complete()` again does not print again. Finally an owner is destroyed while waiting and the fake API completes afterward.

Why is the last case safe here? The awaiter's destructor clears the continuation. The callback still owns the shared completion data and sees no handle to resume. Destruction and callback execution cannot overlap on this single thread. This is not cancellation of underlying work; it only makes this fake late callback harmless. A real operation may still need buffers retained, unregistration, or an acknowledgment that callbacks have stopped.

## Extend the contract, then implement the extension

Try a ready-before-await fast path by creating a named `receive`, setting its operation's `completed` and `value` before awaiting it, and then awaiting that lvalue. `await_ready()` should bypass registration and return the cached value. In a local test, verify the API has no pending callback.

To add errors, put `exception_ptr` beside the value, store one terminal outcome in the callback, and rethrow it from `await_resume()`. Decide whether results are consumed once or shared; moving a result generally implies a single consumer.

To support another thread, replace the plain-field protocol with one atomic/locked state transition that resolves “completed before registration” versus “waiter installed.” Result writes must happen-before result reads. Exactly one path may schedule the continuation. Test ready-before-await, completion during registration, inline/deferred completion, another thread, cancellation before/after registration, owner destruction, duplicate callbacks, and errors. AddressSanitizer and ThreadSanitizer help investigate different failure classes; they do not invent the required protocol.

Configuration stored by an awaiter also needs lifetime: own a timer duration by value rather than retaining a reference to a temporary. Owning configuration still does not make a raw callback pointer into the awaiter safe after destruction.

**Checkpoint:** explain why the callback's shared state survives the abandoned owner, but why it does not extend frame lifetime. That separation is the foundation of a safe adapter.

```bash
python3 tools/check_labs.py 08
```

Next: [Lesson 9 — Await another coroutine and get its result](09_creating_coroutine_types.md).
