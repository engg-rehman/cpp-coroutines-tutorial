---
title: "C++20 Coroutines: Learn by Building"
description: "A hands-on C++20 coroutine workshop: start with compiler errors and build generators, tasks, an event loop, cancellation, and a bounded data-processing application."
keywords:
  - hands-on C++20 coroutines
  - co_return compiler errors
  - coroutine workshop
seo:
  type: WebSite
permalink: /
---

# Learn coroutines with an editor and a compiler

Write `void hello() { co_return; }` and compile it. It fails. Fix that error, compile again, and discover the next requirement. That is how this twelve-lesson workshop starts.

Later you will investigate programs that compile but never start, children that finish without resuming their parent, and references that outlive the objects they refer to. Each experiment introduces the concept needed to explain and repair what you observed.

The destination is a small file-processing application built from your own generator, task, event loop, and cancellation checks. Every lesson includes commands, saved code checkpoints, expected observations, and experiments with explained outcomes.

## Follow the workshop

1. [Make `co_return` compile](01_what_are_coroutines.md)
2. [Pause, resume, and retain local state](02_why_coroutines.md)
3. [Trace creation and cleanup](03_coroutine_lifecycle.md)
4. [Repair lifetime assumptions](04_coroutine_frame.md)
5. [Build an awaiter](05_suspension_resumption.md)
6. [Adapt an awaitable](06_coawait_mechanics.md)
7. [Pull values from a generator](07_coyield_generators.md)
8. [Arrange the next resume](08_creating_awaitables.md)
9. [Compose result-bearing tasks](09_creating_coroutine_types.md)
10. [Repair queued-handle ownership](10_coroutine_handle.md)
11. [Handle errors and cancellation](11_exception_handling.md)
12. [Build a bounded reading report](12_practical_patterns.md)

Begin with [Lesson 1](01_what_are_coroutines.md). Keep the [roadmap and concept coverage map](00_ROADMAP.md) nearby. The [checkpoint guide](labs/README.md) explains how to recover a step and verify your progress.
