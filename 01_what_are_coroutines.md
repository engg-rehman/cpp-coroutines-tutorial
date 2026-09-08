---
title: "What Are C++20 Coroutines?"
description: "Understand C++20 coroutines, coroutine frames, promise_type, return objects, handles, co_await, co_yield, and co_return."
keywords:
  - what are C++ coroutines
  - C++20 coroutines
  - promise_type
  - co_await
  - co_yield
  - coroutine_handle
seo:
  type: TechArticle
---

# Lesson 1: What Are Coroutines?

## The short definition

A coroutine is a function whose execution can be suspended and later resumed. While suspended, its live state is retained in a **coroutine state**, commonly called the coroutine frame.

A function is a coroutine if its body contains any of:

- `co_await`: await an operation, possibly suspending;
- `co_yield`: publish a value and await the promise's yield operation;
- `co_return`: complete through the promise object.

These keywords trigger a compile-time transformation. They do not create a thread, event loop, or scheduler.

## First working example

The complete program is [`examples/manual_task.cpp`](examples/manual_task.cpp). Its important pieces are:

```cpp
class manual_task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        manual_task get_return_object() noexcept {
            return manual_task{handle_type::from_promise(*this)};
        }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { std::terminate(); }
    };

    // The full example makes this owner move-only and destroys the frame.
};
```

The coroutine itself is ordinary-looking code:

```cpp
manual_task demonstration() {
    std::cout << "coroutine: start\n";
    co_await std::suspend_always{};
    std::cout << "coroutine: resumed\n";
}
```

Because `initial_suspend()` returns `suspend_never`, the body starts during the call. `suspend_always` then gives control back. The owner later calls `resume()`. Because `final_suspend()` suspends, the owner can safely call `destroy()` in its destructor.

Expected output:

```text
coroutine: start
main: before resume
coroutine: resumed
main: after resume
```

## Five distinct objects and ideas

Keeping these names separate prevents most confusion.

1. **Coroutine function**: the source-level function containing a `co_` keyword.
2. **Coroutine state/frame**: storage for the promise, parameter copies, live locals, awaiters, and implementation bookkeeping.
3. **Promise object**: an object inside the state that customizes creation, initial/final suspension, result storage, yielding, and uncaught exceptions.
4. **Return object**: the ordinary object returned to the caller by `get_return_object()`—for example, `task<int>` or `generator<int>`.
5. **Handle**: a small, non-owning `std::coroutine_handle<Promise>` referring to the state.

The “promise” here is a coroutine protocol object. It is not `std::promise`.

## How the promise type is selected

For a coroutine declared as `R function(P1, P2)`, the compiler obtains the promise type from:

```cpp
std::coroutine_traits<R, P1, P2>::promise_type
```

For a non-static member coroutine, the implicit object-parameter type is included before the explicit parameter types. The primary `coroutine_traits` template normally finds `R::promise_type`, which is why return wrappers usually contain a nested type:

```cpp
struct task {
    struct promise_type { /* protocol */ };
};
```

Specializing `std::coroutine_traits` for suitable user-defined types is possible, but a nested `promise_type` is clearer for most designs.

## The three keywords

### `co_await expression`

Evaluates an awaitable, asks whether it is ready, and may suspend. It can produce a value:

```cpp
int bytes = co_await async_read(buffer);
```

“Await” does not imply suspension. If `await_ready()` is true, execution continues synchronously.

### `co_yield expression`

Conceptually equivalent to:

```cpp
co_await promise.yield_value(expression);
```

The promise chooses how to store or expose the yielded value. Generators normally return `suspend_always` from `yield_value()`.

### `co_return expression`

Calls `promise.return_value(expression)` and transfers control to final suspend. A valueless `co_return;` calls `promise.return_void()`.

A coroutine cannot use an ordinary `return` statement. Falling off the end is equivalent to `co_return;` only if calling `return_void()` is valid; otherwise it is undefined behavior.

## Coroutines versus ordinary functions

| Property | Ordinary function | Coroutine |
|---|---|---|
| execution | call to return/throw in one activation | may suspend and resume multiple times |
| live locals | ordinary activation storage | retained in coroutine state when needed |
| result protocol | language return object | promise-defined return/yield behavior |
| caller receives | declared return value | object produced by `get_return_object()` |
| scheduling | runs on caller's thread | still library-defined; may resume on any arranged thread |

A coroutine is not necessarily asynchronous. A generator is a coroutine but typically runs synchronously when its consumer asks for the next value.

## Important restrictions

- `main` cannot be a coroutine.
- constructors and destructors cannot be coroutines.
- C++20 coroutines cannot be `constexpr` or `consteval` functions.
- the declared return type cannot use a placeholder such as an undeduced `auto`;
- a C-style variadic parameter list (`...`) is not allowed; a variadic template is allowed.
- `co_await` is restricted to suspension contexts; among other things, it cannot appear in a default argument or directly inside an exception handler.

## Check your understanding

For `examples/manual_task.cpp`, answer:

1. Who owns the coroutine state?
2. Why must that owner be move-only?
3. What would become unsafe if `final_suspend()` returned `suspend_never`?
4. Which line causes actual suspension, and which line merely requests resumption?

Next: [Lesson 2 — Why Coroutines?](02_why_coroutines.md)
