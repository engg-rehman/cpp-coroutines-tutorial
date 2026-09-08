---
title: "How co_await Works in C++20"
description: "A precise guide to co_await operand transformation, await_transform, operator co_await, awaiter selection, and symmetric task transfer."
keywords:
  - how co_await works
  - C++20 co_await
  - await_transform
  - operator co_await
  - C++ awaiter selection
seo:
  type: TechArticle
---

# Lesson 6: `co_await` Mechanics

This lesson gives the complete selection order. The ordering matters because `await_transform()` was commonly explained incorrectly in early coroutine material.

## Stage 1: transform the operand

For an ordinary source-level expression:

```cpp
co_await expression
```

the compiler first looks for `await_transform` in the promise type.

- If the promise has a matching `await_transform`, the awaitable is `promise.await_transform(expression)`.
- If the name is not found, the awaitable is `expression` unchanged.
- If the promise declares `await_transform` but the call is not viable, this is generally a compile error; it does not fall back to the unchanged expression.

The implicit awaits used for `initial_suspend`, `final_suspend`, and `co_yield` do not go through this ordinary `await_transform` step.

Example:

```cpp
struct promise_type {
    auto await_transform(milliseconds duration) {
        return timer_awaiter{duration};
    }
};

task<void> example() {
    co_await 50ms; // promise.await_transform(50ms)
}
```

`await_transform` is a policy hook for a particular coroutine type. It can adapt domain values, attach tracing/context, or restrict which operations may be awaited.

## Stage 2: select the awaiter

Given the awaitable from Stage 1, the language enumerates applicable member and non-member `operator co_await` functions and uses overload resolution to select the best one. If no viable function exists, it uses the awaitable itself as the awaiter. Ambiguous overload resolution is a compile error.

The result must support the three awaiter expressions.

### Direct awaiter

```cpp
struct immediate_int {
    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    int await_resume() const noexcept { return 42; }
};
```

### Member conversion

```cpp
struct cached_value {
    int value;

    immediate_int operator co_await() const noexcept {
        return {value};
    }
};
```

### Non-member conversion

```cpp
struct request { int id; };

request_awaiter operator co_await(request value) {
    return request_awaiter{value.id};
}
```

There is no standard function named `get_awaiter`. Libraries often write one to emulate Stage 2, but it does not perform the promise's Stage 1 transformation unless the library explicitly supplies the promise.

## Stage 3: run the awaiter protocol

After materializing the selected awaiter:

1. evaluate `await_ready()`;
2. if false, suspend and evaluate `await_suspend(current_handle)`;
3. obey the `void`, `bool`, or handle return rule;
4. evaluate `await_resume()` when the current coroutine continues.

The awaiter's lifetime crosses suspension. If it is a temporary, the compiler preserves it until the full await-expression completes.

## `await_transform` forwarding

Once a promise declares `await_transform`, it often needs a forwarding overload so unrelated awaitables still work:

```cpp
struct promise_type {
    timer_awaiter await_transform(milliseconds value) {
        return timer_awaiter{value};
    }

    template <class Awaitable>
    Awaitable&& await_transform(Awaitable&& value) noexcept {
        return std::forward<Awaitable>(value);
    }
};
```

This forwarding example preserves references and therefore preserves their lifetime risks. A tracing wrapper must also preserve:

- the inner awaiter's value category and lifetime;
- all three legal `await_suspend()` return types;
- `noexcept` behavior where required;
- reference results from `await_resume()`;
- synchronization and thread-affinity contracts.

A simplistic logger that stores `decltype(awaiter)` when that type is a reference can easily dangle.

## `co_yield` expansion

For a promise object `promise`, this:

```cpp
co_yield value;
```

behaves like an await on:

```cpp
promise.yield_value(value)
```

It does not call `return_value`. A generator promise normally stores or references the yielded value and returns `suspend_always`, allowing the consumer to inspect it before resuming the producer.

## Initial and final awaits are special

The compiler also awaits:

```cpp
promise.initial_suspend();
promise.final_suspend();
```

These use the awaiter protocol but bypass ordinary `await_transform`. The final await must not be potentially throwing. Returning a continuation handle from a custom final awaiter is the core of task chaining:

```cpp
struct final_awaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(handle_type self) const noexcept {
        return self.promise().continuation;
    }

    void await_resume() const noexcept {}
};
```

The current coroutine is at final suspend; the returned continuation is resumed. The completed frame remains owned and must later be destroyed.

## Placement restrictions

An await-expression belongs inside a suspension context of a coroutine or coroutine lambda. Important restrictions include:

- not in a default argument;
- not in a static or thread-local block-variable initializer;
- not directly in an exception handler;
- in declaration statements, only in the initializer;
- not in an unevaluated or otherwise disallowed surrounding context.

If cleanup needs asynchronous work after a catch, store the error information and perform the `co_await` after leaving the handler:

```cpp
std::exception_ptr error;
try {
    co_await operation();
} catch (...) {
    error = std::current_exception();
}

if (error) {
    co_await async_cleanup();
}
```

## Symmetric transfer in a task chain

[`examples/task_chain.cpp`](examples/task_chain.cpp) uses two transfers:

1. the parent task's awaiter stores the parent as the child's continuation and returns the child handle;
2. the child's final awaiter returns the stored parent handle.

The transfer path is:

```text
caller resumes parent
  -> parent awaits child and transfers to child
  -> child completes and transfers to parent
  -> parent receives child result and completes
  -> parent final suspend returns noop handle
  -> control returns to caller
```

This design needs a clear ownership rule. In the example, the temporary child `task` object lives across its `co_await` expression in the parent frame and destroys the completed child state after `await_resume()`.

## Common mistakes

1. Applying `operator co_await` before `await_transform`.
2. Assuming any type with `await_ready()` bypasses a promise's `await_transform`.
3. Calling `resume()` from `await_suspend()` and then accessing the awaiter; resumption may destroy it.
4. Returning the awaiting coroutine's own handle from `await_suspend()` without a carefully proved protocol; this can cause reentrancy or invalid self-resumption.
5. Storing references in an awaiter without ensuring the referents outlive the await-expression.
6. Treating a handle returned from `await_suspend()` as owned by the awaiter.

Next: [Lesson 7 — `co_yield` and Generators](07_coyield_generators.md)
