---
title: "C++ Coroutine Tutorial Correctness Audit"
description: "Technical audit notes for the C++20 coroutine tutorial."
robots: "noindex, follow"
sitemap: false
---

# Correctness Audit

## Verdict

The original tutorial had a useful lesson order and good intuition, but several complete examples did not compile and several lifetime claims were unsafe. The tutorial has been revised so that its canonical examples compile under C++20 and ownership is explicit.

## Important corrections

| Original issue | Correction |
|---|---|
| examples returned a raw `coroutine_handle` where the declared return type expected a wrapper | `get_return_object()` now returns the declared wrapper type |
| `task<void>` reused a `T value`/`return_value` promise | void and value-returning behavior are no longer mixed |
| wrappers owning handles were implicitly copyable | owning wrappers are move-only |
| a wrapper destroyed a handle after `final_suspend = suspend_never` | owning examples use `suspend_always` at final suspend |
| examples accessed a promise after automatic frame destruction | results are read only while an owning, suspended frame exists |
| generator state and completion checks were stale/unsafe | the generator is lazy, checks `done()` before and after resumption, owns its frame, and rethrows stored exceptions |
| every frame was said to be heap allocated | allocation is dynamic in the abstract model but can be elided and embedded by the implementation |
| frame layout and promise offset were presented as portable facts | layouts are explicitly implementation details |
| parameters were described as simply copied | non-reference parameter copies are initialized from an xvalue; reference parameters remain references |
| `await_transform` was placed after `operator co_await` | the promise transformation happens first, then member/free `operator co_await` selection |
| `await_suspend(false)` was described as the compiler calling `resume()` | it cancels suspension and execution continues without a separate `resume()` call |
| a handle-returning `await_suspend` was called guaranteed tail-call optimization | it is symmetric transfer semantically; stack behavior is an implementation concern |
| handles were described as pointers to promises | handles are opaque, non-owning references to coroutine state |
| cross-thread resumption omitted publication races and owner lifetime | the revised text explains synchronization, thread migration, and use-after-free hazards |
| arbitrary frame/thread sizes and “millions” claims were presented as guarantees | performance claims are now measurement-based and qualified |

## Knowledge gaps filled

- `std::coroutine_traits` and promise selection
- valid coroutine contexts and restrictions
- allocation elision, custom allocation, and allocation failure hooks
- exact `co_await` transformation order
- full `await_suspend` return semantics
- symmetric transfer and continuations
- move-only ownership and handle preconditions
- generator completion and exception behavior
- exception transport through `unhandled_exception()`/`await_resume()`
- cancellation as a library protocol rather than a language keyword
- scheduler affinity, reentrancy, data races, abandonment, and structured concurrency
- debugging and performance measurement

## Validation

The programs in `examples/` are built with C++20 and warning flags. CMake/CTest provides a repeatable check. Explanatory pseudocode is clearly labelled rather than presented as a complete program.
