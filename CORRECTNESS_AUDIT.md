---
title: "C++ Coroutine Tutorial Correctness Audit"
description: "Technical audit notes for the C++20 coroutine tutorial."
robots: "noindex, follow"
sitemap: false
---

# Correctness Audit

## Current workshop validation

All twelve lessons now teach through edits, compiler diagnostics, execution traces,
and repairs. The [coverage map](00_ROADMAP.md) tracks the original concepts through
the new sequence. Complete teaching checkpoints live in `labs/`; `examples/`
remains a compact supplementary reference.

The workshop has 49 checked scenarios. Intentionally invalid sources must fail
compilation with a relevant diagnostic; runnable checkpoints must match their
documented output and exit status. Unsafe lifetime fragments are explicitly
labelled in the prose and are not executed by the checker.

Validated locally with Apple Clang 21 and C++20:

```bash
python3 tools/check_labs.py
python3 tools/check_labs.py --sanitize
cmake -S . -B build-workshop
cmake --build build-workshop
ctest --test-dir build-workshop --output-on-failure
```

The combined CTest run passes all 17 tests (five supplementary examples and twelve
lesson checks). The runnable workshop programs also pass AddressSanitizer and
UndefinedBehaviorSanitizer checks. Relative documentation links and checkpoint
paths were checked. This is local compiler validation, not a claim of testing
every compiler or a production concurrent runtime.

The new generator permits throwing value construction in `yield_value`; the same
unconditional `noexcept` was removed from the supplementary generic generator.
The workshop task enforces one start/consumer, includes `task<void>`, and retains
the child owner through its await-expression. Lesson 10's loop unregisters queued
handles during frame destruction under an explicit single-thread contract.

## Earlier audit context

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

## Earlier example validation

The programs in `examples/` are built with C++20 and warning flags. CMake/CTest provides a repeatable check. Explanatory pseudocode is clearly labelled rather than presented as a complete program.
