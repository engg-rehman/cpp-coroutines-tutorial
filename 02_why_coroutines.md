---
title: "Why Use Coroutines in C++?"
description: "Compare C++20 coroutines with callbacks, threads, futures, and explicit state machines, including their costs and suitable use cases."
keywords:
  - why use C++ coroutines
  - coroutines vs threads
  - coroutines vs callbacks
  - std::future
  - asynchronous C++
seo:
  type: TechArticle
---

# Lesson 2: Why Coroutines?

## The problem they address

Many programs start an operation, cannot finish it yet, and need to continue when an event arrives. Examples include socket I/O, timers, UI events, and producing a lazy sequence.

Without coroutines, the state between those events must still live somewhere:

- callbacks put state in captures and move continuation code into callback bodies;
- explicit state machines put state in fields and control flow in a state enum;
- blocking threads let the thread's stack retain the state;
- futures represent eventual values, but C++20 `std::future` has no general continuation or `co_await` integration.

Coroutines let the compiler turn direct-style code into the required state machine.

## The readability gain

Callback-shaped pseudocode:

```cpp
fetch_user(id, [=](result<user> u) {
    if (!u) return finish(u.error());
    fetch_posts(u->id, [=](result<posts> p) {
        if (!p) return finish(p.error());
        finish(render(*u, *p));
    });
});
```

Coroutine-shaped pseudocode:

```cpp
task<page> make_page(int id) {
    user u = co_await fetch_user(id);
    posts p = co_await fetch_posts(u.id);
    co_return render(u, p);
}
```

The second version can use ordinary scopes, loops, RAII objects, and `try`/`catch`. The generated coroutine state retains variables that must survive suspension.

This is the main benefit: composable control flow. It is not automatic speed.

## What C++20 does and does not provide

C++20 provides:

- syntax and transformation rules;
- `std::coroutine_handle`;
- `std::coroutine_traits`;
- `std::suspend_always`, `std::suspend_never`, and `std::noop_coroutine`.

C++20 does not provide:

- `task<T>` or a standard generator;
- an event loop or thread pool;
- async file/network operations;
- cancellation propagation;
- `when_all`, timeouts, or structured concurrency.

Those are library responsibilities. Consequently, code from two coroutine libraries is not automatically composable merely because both use `co_await`.

## Concurrency, parallelism, and asynchrony

These terms are related but different.

- **Suspension**: a coroutine has saved its state and is not executing.
- **Concurrency**: multiple operations are in progress over an interval.
- **Parallelism**: multiple operations execute simultaneously on different processing resources.
- **Asynchrony**: completion is reported later rather than by blocking the current call.

A coroutine runs ordinary instructions on exactly one thread at a time. It runs until it suspends, returns, or throws. A scheduler may later resume it on the same thread or another thread. The coroutine language itself promises neither concurrency nor parallelism.

## Comparison with alternatives

| Technique | Strength | Cost or limitation |
|---|---|---|
| callback | direct fit for many C APIs; no special return type | composition, error paths, and ownership can become scattered |
| explicit state machine | maximum control; excellent for small protocols | verbose; every live value and transition is manual |
| one blocking thread per operation | simple sequential code; mature tooling | stack reservation, scheduling, synchronization, and thread-count limits |
| `std::future` | standard result and exception transport | C++20 future composition is limited; `get()` blocks |
| coroutine | direct-style composition with explicit suspension points | requires library infrastructure; lifetime bugs remain possible |

Threads and coroutines are not mutually exclusive. A common runtime uses a bounded thread pool to execute many coroutine continuations.

## Good fits

- high-concurrency I/O where an async API already exists;
- generators and lazy traversal;
- protocol parsers and other suspendable state machines;
- UI or game flows that resume on events;
- composing async operations with clear sequential control flow.

## Poor fits

- a small synchronous operation that never waits;
- CPU-bound work when no scheduler distributes work across cores;
- code whose dependencies expose only blocking APIs, unless blocking is moved elsewhere;
- hard real-time code without a runtime designed and proven for its timing constraints;
- cases where an explicit state machine is smaller and clearer.

## Costs to budget for

Coroutine performance depends on the abstraction and compiler. Potential costs include:

- allocation or embedding of coroutine state;
- retained locals increasing state size;
- reference counting or synchronization in library task types;
- scheduler queueing and thread migration;
- loss of locality when operations resume much later;
- cancellation and shutdown bookkeeping.

Do not use fixed claims such as “a coroutine is 1 KB” or “millions are always cheap.” Measure the implementation, workload, compiler, optimization level, and runtime.

## Sequential awaits are still sequential

This code starts the second operation only after the first finishes unless the API starts work when called:

```cpp
auto a = co_await fetch(1);
auto b = co_await fetch(2);
```

Concurrent composition usually needs a library primitive and explicit lifetime semantics:

```cpp
auto a = fetch(1);              // perhaps starts now; library-defined
auto b = fetch(2);
auto [ra, rb] = co_await when_all(std::move(a), std::move(b));
```

The details of start policy, cancellation, and exception aggregation belong to the task library.

## Decision checklist

Before selecting coroutines, ask:

1. Which operations can truly complete asynchronously?
2. Which executor resumes each continuation?
3. Who owns each outstanding operation and coroutine state?
4. How does cancellation reach child operations?
5. What happens during shutdown?
6. How are errors delivered at an await boundary?

If those answers are missing, coroutine syntax alone has not solved the system design.

Next: [Lesson 3 — Coroutine Lifecycle](03_coroutine_lifecycle.md)
