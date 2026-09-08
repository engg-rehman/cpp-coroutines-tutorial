---
title: "Building task<T> with C++20 Coroutines"
description: "Build an educational lazy C++ task<T> with promise_type, result and exception storage, continuations, final suspend, and symmetric transfer."
keywords:
  - C++ task coroutine
  - task<T> C++20
  - promise_type implementation
  - coroutine continuation
  - lazy task C++
seo:
  type: TechArticle
---

# Lesson 9: Creating a `task<T>`

A `task<T>` represents one eventual result and is itself awaitable. This lesson explains the educational implementation in [`examples/task.hpp`](examples/task.hpp).

## Design choices first

Before writing promise methods, decide:

- eager or lazy start;
- single-consumer or multi-consumer;
- who owns the frame;
- how completion resumes a continuation;
- where exceptions are stored and rethrown;
- whether destruction cancels, detaches, or requires completion;
- which executor is used.

The example chooses a lazy, move-only, single-consumer task. Its wrapper owns the frame. It supports synchronous task chains but has no general I/O scheduler.

## Promise storage

```cpp
std::optional<T> value;
std::exception_ptr exception;
std::coroutine_handle<> continuation = std::noop_coroutine();
```

Exactly one of `value` or `exception` should describe a completed result. The continuation is the coroutine awaiting this task.

`std::optional<T>` avoids requiring `T` to be default-constructible. More sophisticated storage may use a discriminated union to encode empty/value/error states.

## Creating the return object

```cpp
task get_return_object() noexcept {
    return task{handle_type::from_promise(*this)};
}
```

This converts a reference to the promise into a typed handle, then places that handle in the owning wrapper. The wrapper must be move-only and must clear the moved-from handle with `std::exchange`.

## Lazy start

```cpp
std::suspend_always initial_suspend() const noexcept { return {}; }
```

Calling the coroutine constructs and returns the task without running the body. Awaiting or explicitly running the task starts it.

## Completing with a value

```cpp
template <class U>
void return_value(U&& result) {
    value.emplace(std::forward<U>(result));
}

void unhandled_exception() noexcept {
    exception = std::current_exception();
}
```

The forwarding `return_value` allows construction from compatible lvalues and rvalues. A production implementation should constrain it so diagnostics are clear.

A promise must not declare both `return_value` and `return_void`. `task<void>` therefore needs a distinct promise or specialization whose completion hook is `return_void()` and which stores no `T`.

## Final suspend and continuation

```cpp
struct final_awaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(handle_type current) const noexcept {
        return current.promise().continuation;
    }

    void await_resume() const noexcept {}
};
```

On completion, the child remains suspended at final suspend and symmetrically transfers to its continuation. The child's owner later destroys its frame.

The noop default handles the case where a task is driven directly rather than awaited by another coroutine.

## Awaiting a task

```cpp
struct awaiter {
    handle_type handle;

    bool await_ready() const noexcept {
        return !handle || handle.done();
    }

    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<> continuation) const noexcept {
        handle.promise().continuation = continuation;
        return handle;
    }

    T await_resume() const {
        if (!handle) {
            throw std::logic_error("awaited an empty task");
        }
        auto& promise = handle.promise();
        if (promise.exception) {
            std::rethrow_exception(promise.exception);
        }
        return std::move(*promise.value);
    }
};
```

The parent stores itself as the continuation, then transfers execution to the lazy child. When the child completes, final suspend transfers back. `await_resume()` rethrows or moves out the result.

Because the result is moved and there is only one continuation slot, this is a single-consumer task. Awaiting it twice or concurrently is outside the contract.

## Temporary child lifetime

This is safe under the example's ownership model:

```cpp
task<int> parent() {
    int value = co_await child();
    co_return value + 1;
}
```

The temporary child task is part of the suspended await-expression and remains alive in the parent state. After the child transfers back and `await_resume()` finishes, the temporary is destroyed and destroys the completed child frame.

## Why `run()` is deliberately limited

The example's `run()` resumes once and requires the chain to reach completion through symmetric transfers. It works for `task_chain.cpp`, where every operation is immediately driven by the chain.

If a child suspends for a timer or queued I/O, `run()` reports that an external scheduler is required. A real `sync_wait` needs a completion event/condition variable and must avoid deadlocking the executor needed to finish the operation.

## `task<void>` sketch

The void specialization differs primarily here:

```cpp
struct promise_type {
    std::exception_ptr exception;
    std::coroutine_handle<> continuation = std::noop_coroutine();

    void return_void() const noexcept {}
    void unhandled_exception() noexcept {
        exception = std::current_exception();
    }
    // same return-object, initial-suspend, and final-awaiter pattern
};
```

Its `await_resume()` returns `void` after rethrowing any stored exception.

## Production features omitted

- prevention/detection of a second await;
- allocator support and allocation failure policy;
- cancellation and stop-token propagation;
- executor/context propagation;
- safe detachment or structured child ownership;
- shared/multi-consumer results;
- cold-task start races;
- tracing and debugging metadata;
- stack-depth guarantees across arbitrary task graphs.

The lesson is a protocol demonstration, not a replacement for a mature coroutine library.

Next: [Lesson 10 — Handles and Ownership](10_coroutine_handle.md)
