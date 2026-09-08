---
title: "C++20 Coroutines Tutorial"
description: "Learn C++20 coroutine syntax, lifecycle, frames, awaiters, generators, task composition, cancellation, ownership, and safe production patterns."
keywords:
  - C++20 coroutines tutorial
  - learn C++ coroutines
  - C++ async programming
  - co_await tutorial
  - C++ generator
seo:
  type: WebSite
permalink: /
---

# C++20 Coroutines Tutorial

Learn C++20 coroutines from the language mechanics through safe task composition. The tutorial distinguishes coroutines, awaiters, and runtimes, then makes frame ownership, lifetime, exception, cancellation, and scheduling rules explicit.

## Start learning

1. [What are coroutines?](01_what_are_coroutines.md)
2. [Why coroutines?](02_why_coroutines.md)
3. [Coroutine lifecycle](03_coroutine_lifecycle.md)
4. [Coroutine state, storage, and lifetime](04_coroutine_frame.md)
5. [Suspension and resumption](05_suspension_resumption.md)
6. [`co_await` mechanics](06_coawait_mechanics.md)
7. [`co_yield` and generators](07_coyield_generators.md)
8. [Creating awaitables](08_creating_awaitables.md)
9. [Creating a `task<T>`](09_creating_coroutine_types.md)
10. [`std::coroutine_handle` and ownership](10_coroutine_handle.md)
11. [Exceptions, cleanup, and cancellation](11_exception_handling.md)
12. [Practical coroutine design](12_practical_patterns.md)

The [learning roadmap](00_ROADMAP.md) suggests checkpoints and experiments. Complete, compilable examples live in the [`examples/`](examples/) directory.

## What you will understand

- How the compiler selects `promise_type` and transforms a coroutine.
- How `await_ready`, `await_suspend`, and `await_resume` control suspension.
- Why coroutine handles are non-owning and when `destroy()` is valid.
- How generators and lazy `task<T>` types work.
- How exceptions, cancellation, schedulers, callback races, and structured concurrency affect production code.
