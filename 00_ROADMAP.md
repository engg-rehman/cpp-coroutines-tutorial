---
title: "C++ Coroutines Learning Roadmap"
description: "A structured path for learning C++20 coroutine mechanics, awaitables, generators, task types, ownership, cancellation, and practical design."
keywords:
  - C++ coroutines roadmap
  - learn C++20 coroutines
  - coroutine tutorial
  - asynchronous C++
seo:
  type: TechArticle
---

# C++ Coroutines: Learning Roadmap

## Prerequisites

You should be comfortable with RAII, move-only types, templates, exceptions, threads, and basic asynchronous programming. Coroutines amplify lifetime mistakes, so review object lifetime and data-race rules if either is unfamiliar.

## Phase 1: Language model

- [Lesson 1](01_what_are_coroutines.md): coroutine, promise, return object, awaitable, and awaiter
- [Lesson 2](02_why_coroutines.md): suitable problems and honest tradeoffs
- [Lesson 3](03_coroutine_lifecycle.md): creation through destruction
- [Lesson 4](04_coroutine_frame.md): storage, parameter copies, allocation, and lifetime
- [Lesson 5](05_suspension_resumption.md): the awaiter protocol and control transfer
- [Lesson 6](06_coawait_mechanics.md): exact awaiter selection and edge cases

Checkpoint: you should be able to trace who owns a frame and where control goes for each possible `await_suspend()` return type.

## Phase 2: Build abstractions

- [Lesson 7](07_coyield_generators.md): a pull generator
- [Lesson 8](08_creating_awaitables.md): ready, scheduling, and callback-backed awaiters
- [Lesson 9](09_creating_coroutine_types.md): a lazy `task<T>` with continuations
- [Lesson 10](10_coroutine_handle.md): typed/untyped handles and ownership designs

Checkpoint: you should be able to state who destroys every frame and why no queued handle can outlive it.

## Phase 3: Operate safely

- [Lesson 11](11_exception_handling.md): exception transport, cleanup, and cooperative cancellation
- [Lesson 12](12_practical_patterns.md): structured concurrency, scheduling, performance, and debugging

Checkpoint: you should be able to explain cancellation and shutdown behavior, not just the success path.

## Suggested experiments

1. Add logging to every promise and awaiter method in `examples/manual_task.cpp`.
2. Make `generator.cpp` finite and throw after several values; observe where the exception reappears.
3. Add a third level to `task_chain.cpp` and trace symmetric transfer.
4. Intentionally destroy the owner before `event_loop::run()`—reason about the dangling queued handle, but do not rely on a crash to diagnose it.
5. Build with AddressSanitizer and UndefinedBehaviorSanitizer while experimenting.

## Reference documents

- [C++ working draft: coroutine definitions](https://eel.is/c++draft/dcl.fct.def.coroutine)
- [C++ working draft: await expressions](https://eel.is/c++draft/expr.await)
- [C++ working draft: `co_return`](https://eel.is/c++draft/stmt.return.coroutine)
- [cppreference coroutine overview](https://en.cppreference.com/w/cpp/language/coroutines)

The working draft is normative-style wording and can be difficult to read. Use it to settle edge cases; use this tutorial to build the mental model.
