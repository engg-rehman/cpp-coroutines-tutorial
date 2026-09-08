---
title: "C++ Coroutine Suspension and Resumption"
description: "Master the C++ awaiter protocol: await_ready, await_suspend, await_resume, symmetric transfer, publication races, and handle preconditions."
keywords:
  - C++ awaiter protocol
  - await_ready
  - await_suspend
  - await_resume
  - coroutine resumption
  - symmetric transfer
seo:
  type: TechArticle
---

# Lesson 5: Suspension and Resumption

## The awaiter protocol

After awaitable conversion (Lesson 6), `co_await` operates on an awaiter with three expressions:

```cpp
struct awaiter {
    bool await_ready();
    void await_suspend(std::coroutine_handle<> awaiting);
    result_type await_resume();
};
```

The exact member qualifiers and handle type can differ. `await_ready()` must be contextually convertible to `bool`. `await_suspend()` may accept a typed handle such as `std::coroutine_handle<promise_type>` and may return `void`, `bool`, or a coroutine handle. `await_resume()` can return `void`, a value, or a reference.

## `await_ready()`

If it returns `true`, the coroutine does not suspend and `await_resume()` is evaluated immediately:

```cpp
struct ready_value {
    int value;
    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    int await_resume() const noexcept { return value; }
};
```

`await_suspend()` is not called on this path. Calling `await_ready()` an “optimization only” is incomplete: it is also part of the awaiter's observable semantics and may represent an already-completed operation.

## Suspension happens before `await_suspend()`

If `await_ready()` is false, the coroutine is considered suspended before `await_suspend()` is evaluated. This ordering lets `await_suspend()` safely publish the handle to a scheduler or callback system.

Conceptual pseudocode—not valid C++ source—is:

```text
awaiter = select_awaiter(operand)
if awaiter.await_ready() is false:
    save resume state; coroutine is now suspended
    evaluate awaiter.await_suspend(current_handle)
    follow its return-type rule
evaluate awaiter.await_resume()
```

The call to `await_resume()` happens after either no suspension, cancellation of suspension, or later resumption.

## Three `await_suspend()` return forms

### `void`

```cpp
void await_suspend(std::coroutine_handle<> current) {
    queue.push(current);
}
```

When it returns normally, control returns to the current coroutine's caller or resumer. Some other agent must eventually resume the handle.

### `bool`

```cpp
bool await_suspend(std::coroutine_handle<> current) {
    if (register_waiter(current)) {
        return true;  // remain suspended
    }
    return false;     // cancel suspension; continue this coroutine
}
```

Returning `false` does not mean “the compiler calls `current.resume()`.” Semantically, suspension is cancelled and execution continues with `await_resume()` on the current execution path. Returning `true` leaves the coroutine suspended.

This form is useful for closing a race: the operation may become ready between `await_ready()` and waiter registration.

### another coroutine handle

```cpp
std::coroutine_handle<> await_suspend(
    std::coroutine_handle<> continuation) noexcept {
    child.promise().continuation = continuation;
    return child;
}
```

The returned coroutine is resumed. The current coroutine remains suspended. This semantic pattern is called **symmetric transfer** and is useful for task chaining. It avoids a mandatory round-trip through user scheduler code; whether the implementation performs a literal tail call is not a portable guarantee.

`std::noop_coroutine()` is a valid returned handle that safely does nothing when resumed.

## `await_resume()`

This expression determines the type and value category of the entire `co_await` expression:

```cpp
int bytes = co_await async_read();
// `bytes` is initialized from the selected awaiter's await_resume().
```

Result-bearing tasks typically inspect the promise here, rethrow a stored exception if present, and otherwise return the stored result.

Do not assume “resume after the `co_await` line” means `await_resume()` has already run. `await_resume()` is the last part of evaluating that line.

## Exceptions from the protocol

- If awaitable conversion or `await_ready()` throws, no suspension has occurred and the exception is thrown at the `co_await` expression.
- If `await_suspend()` throws, the coroutine catches that machinery-level failure, resumes itself, and the exception is thrown from the `co_await` expression. `await_resume()` is not used for that failed await.
- If `await_resume()` throws, it behaves like any expression throwing in the coroutine body.

If none is caught inside the body, the generated outer handler calls `promise.unhandled_exception()`.

## The publication race

An awaiter may transfer a handle to another thread inside `await_suspend()`. That thread is allowed to resume the coroutine before `await_suspend()` returns. Resumption may run to completion and another owner may destroy the frame.

Therefore, after publishing the handle, treat `*this` and every reference into the coroutine frame as potentially destroyed:

```cpp
void await_suspend(std::coroutine_handle<> handle) {
    operation->set_continuation(handle);
    operation->start(); // may complete and resume immediately
    // Do not access awaiter members here unless the protocol guarantees safety.
}
```

Real callback-backed awaiters usually need atomic state or a lock to resolve “completed” versus “waiter installed.” Memory ordering must make the result visible before resumption.

## Resuming on another thread

Cross-thread resumption is possible, but it changes the execution context:

- thread-local state may differ;
- a mutex must not be held across suspension and unlocked on an incompatible thread;
- UI objects may require resumption on the UI thread;
- concurrent attempts to resume the same coroutine are a data race and usually undefined behavior at a higher level.

In C++20, cross-agent resumption is implementation-defined unless the relevant execution agents are `std::thread`/`std::jthread` instances or the thread executing `main`. Check a runtime's guarantees when callbacks originate from threads not represented by those standard types.

A scheduler awaiter should make thread-affinity behavior part of its contract.

## Handle preconditions

For ordinary coroutine handles:

- `resume()` requires a non-null suspended coroutine not at final suspend;
- `done()` is meaningful for a suspended coroutine and reports final suspension;
- `destroy()` requires a suspended coroutine;
- a copied handle does not extend lifetime.

The checked examples wrap these operations so callers do not casually violate the preconditions.

## Exercise

Read [`examples/event_loop.cpp`](examples/event_loop.cpp) and trace:

1. when the job becomes suspended;
2. when its handle enters and leaves the ready queue;
3. why `work` must outlive `loop.run()`;
4. what ownership protocol would be needed if the queue itself owned scheduled jobs.

Next: [Lesson 6 — `co_await` Mechanics](06_coawait_mechanics.md)
