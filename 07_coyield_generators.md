---
title: "C++ co_yield and Generator Coroutines"
description: "Build a safe, lazy C++20 generator and understand co_yield, yield_value, generator completion, value lifetime, and exception propagation."
keywords:
  - C++ co_yield
  - C++20 generator
  - generator coroutine
  - yield_value
  - lazy sequence C++
seo:
  type: TechArticle
---

# Lesson 7: `co_yield` and Generators

A generator is a coroutine used as a pull-based sequence. The consumer asks for a value, the producer runs until `co_yield`, and then the producer suspends while the consumer observes the yielded value.

## The transformation

For a promise object `promise`:

```cpp
co_yield expression;
```

is defined in terms of:

```cpp
co_await promise.yield_value(expression);
```

`yield_value` usually stores the value and returns `std::suspend_always`. The consumer then reads the stored value before resuming the generator.

## A safe minimal design

The complete implementation is [`examples/generator.cpp`](examples/generator.cpp). Its promise has four important policies:

```cpp
struct promise_type {
    std::optional<T> current;
    std::exception_ptr exception;

    generator get_return_object() noexcept;
    std::suspend_always initial_suspend() const noexcept { return {}; }
    std::suspend_always final_suspend() const noexcept { return {}; }

    std::suspend_always yield_value(T value) noexcept {
        current.emplace(std::move(value));
        return {};
    }

    void return_void() const noexcept {}
    void unhandled_exception() noexcept {
        exception = std::current_exception();
    }
};
```

- `initial_suspend = suspend_always` makes generation lazy.
- `yield_value` publishes one value and suspends.
- `final_suspend = suspend_always` keeps the state valid until the owning generator destroys it.
- `unhandled_exception` stores failure for the consumer.

The consumer operation must check completion both before and after resumption:

```cpp
bool next() {
    if (!handle_ || handle_.done()) {
        return false;
    }

    handle_.resume();

    if (handle_.promise().exception) {
        std::rethrow_exception(handle_.promise().exception);
    }
    return !handle_.done();
}
```

If the producer reaches final suspend without yielding, `next()` returns false. If it throws, `next()` rethrows.

## Why the original eager pattern is fragile

An eager generator (`initial_suspend = suspend_never`) can run and yield before the caller receives its return object. That can be made to work, but it makes `next()` semantics less intuitive. Worse, combining an owning handle with `final_suspend = suspend_never` leaves the owner holding a dangling handle at completion.

The conventional educational generator is lazy and suspends at final suspend.

## Value lifetime

In this tutorial's generator, `value()` returns a reference to storage in the promise:

```cpp
const T& value() const noexcept;
```

That reference is valid only until the next successful `next()` call or destruction of the generator. Consumers should copy it if they need a longer lifetime.

Different designs let `yield_value` store a pointer/reference to an lvalue to avoid copies. Those designs require stronger lifetime rules and careful handling of rvalues. A value-storing promise is easier to teach safely.

## Finite versus infinite generators

```cpp
generator<int> range(int first, int last) {
    for (int value = first; value < last; ++value) {
        co_yield value;
    }
}
```

Falling off this body is valid because the promise supplies `return_void()`. It transitions to final suspend, and the next consumer check reports completion.

An infinite generator, such as Fibonacci in the example, completes only when its owner destroys it. Destruction while suspended runs destructors for all live producer locals.

## Iterator interfaces

A production generator usually exposes `begin()`/`end()`:

- `begin()` resumes to the first value;
- increment resumes to the next value;
- the sentinel compares against completion;
- dereference reads the current promise value;
- exceptions surface during `begin()` or increment.

Such iterators are normally single-pass input iterators. Copying an iterator does not create an independent traversal, because both would refer to one coroutine state.

## `co_yield` plus `co_await`

A coroutine promise may support both operations, but a normal pull generator has no scheduler to complete arbitrary asynchronous waits. Combining them changes the abstraction into an asynchronous generator and requires answers for:

- how the consumer awaits the next value;
- how producer and consumer lifetimes are linked;
- how cancellation wakes a suspended producer;
- which executor resumes each side;
- how backpressure works.

Do not add arbitrary async awaits to a synchronous generator type and assume the consumer's `next()` remains valid.

## Exercise

1. Replace Fibonacci with a finite `range` generator.
2. Throw after yielding three values and observe where the exception appears.
3. Add a destructor-logging local before the loop and destroy the generator early.
4. Explain why calling `value()` before a successful `next()` violates the API contract.

Next: [Lesson 8 — Creating Awaitables](08_creating_awaitables.md)
