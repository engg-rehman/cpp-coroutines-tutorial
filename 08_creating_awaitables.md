---
title: "Creating Custom Awaitables in C++20"
description: "Learn to create C++ awaitables for ready values, event-loop scheduling, and callback APIs while handling completion and cancellation races safely."
keywords:
  - custom C++ awaitable
  - create C++ awaiter
  - callback coroutine adapter
  - event loop coroutine
  - await_suspend race
seo:
  type: TechArticle
---

# Lesson 8: Creating Awaitables

An awaitable should define one precise contract: how readiness is observed, how continuation is registered, what resumes the coroutine, and what result or error is delivered.

## Pattern 1: immediately ready value

```cpp
template <class T>
struct ready_awaitable {
    T value;

    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    T await_resume() { return std::move(value); }
};
```

This never suspends. It is useful for testing and for fast paths, but the coroutine frame may still exist because other suspension points or the return type require it.

## Pattern 2: schedule onto an event loop

[`examples/event_loop.cpp`](examples/event_loop.cpp) contains:

```cpp
struct schedule_awaiter {
    event_loop& loop;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const {
        loop.ready_.push_back(handle);
    }

    void await_resume() const noexcept {}
};
```

The coroutine suspends, and its non-owning handle enters the ready queue. `event_loop::run()` later removes and resumes it.

This tiny example has an essential lifetime condition: the coroutine owner must remain alive while the queue contains its handle. A production runtime might instead make queue entries share ownership with an operation state, or arrange cancellation/removal before the owner can destroy the frame.

## Pattern 3: callback-backed operation

A callback adapter has a race that the tiny event loop does not: completion can occur before, during, or after `await_suspend()` registers the continuation.

A robust design usually has a separately owned operation state:

```text
state contains:
  result or error
  continuation handle
  atomic/locked phase: starting | waiting | completed | cancelled
```

The registration protocol must guarantee exactly one of:

- `await_suspend()` observes completion and returns `false`; or
- completion observes the registered handle and schedules it exactly once.

It must also guarantee that result writes happen-before `await_resume()` reads them.

Naive code is unsafe:

```cpp
void await_suspend(std::coroutine_handle<> handle) {
    async_api([handle](result value) mutable {
        store(value);
        handle.resume();
    });
}
```

Questions left unanswered include:

- What if the API invokes the callback inline?
- What object owns `store`?
- What if the task is destroyed before completion?
- Can cancellation race with completion?
- Which thread invokes `resume()`?
- Can two callbacks resume the same handle?

Use the callback API's documented guarantees and an explicit shared operation state. There is no one universally correct adapter.

## Awaiter storage

An awaiter temporary lives across a genuine suspension, but objects it merely references do not gain lifetime. Prefer values or separately owned state when the operation can outlive the calling scope.

```cpp
struct bad_timer_awaiter {
    const duration& delay; // may dangle
};

struct safer_timer_awaiter {
    duration delay;        // owns its configuration
};
```

Owning configuration is not enough if an external subsystem stores a raw pointer back to the awaiter. Cancellation and destruction must unregister that pointer before the frame is destroyed.

## `await_ready()` and the registration race

A common shape is:

```cpp
bool await_ready() const noexcept {
    return state->is_complete();
}

bool await_suspend(std::coroutine_handle<> handle) {
    return state->try_set_continuation(handle);
}
```

Even after `await_ready()` says false, the operation can complete before `try_set_continuation`. Returning false from `await_suspend()` handles that race without an external resume. The state machine must ensure a completion thread does not also resume the handle.

## Result and error delivery

`await_resume()` should have a clear one-shot or repeatable contract:

```cpp
T await_resume() {
    if (state->error) {
        std::rethrow_exception(state->error);
    }
    return std::move(*state->value);
}
```

If this moves the result, the awaitable is usually single-consumer. A shared task needs immutable/shared result storage and different synchronization.

## Cancellation

Cancellation is not “destroy the coroutine and hope.” An awaitable should define whether cancellation:

- unregisters a callback;
- asks the underlying operation to stop;
- waits for an in-flight callback to finish;
- resumes the coroutine with a cancellation result;
- guarantees no later resumption after destruction.

An RAII cancellation-registration object can live in the awaiter or operation state, but its destructor must be safe under completion races.

## Testing an awaitable

Test at least these schedules:

1. ready before `co_await`;
2. completion between `await_ready` and registration;
3. completion immediately inside registration;
4. completion later on the same thread;
5. completion later on another thread;
6. cancellation before and after registration;
7. owner destruction while suspended;
8. duplicate/spurious callback attempts;
9. result and exception paths.

ThreadSanitizer and AddressSanitizer are particularly valuable for adapters.

Next: [Lesson 9 — Creating Coroutine Types](09_creating_coroutine_types.md)
