# C++20 Coroutines: From Syntax to Safe Types

This tutorial explains the C++20 coroutine language machinery and then builds small, owning coroutine types. It deliberately separates three ideas that are often blurred together:

- a **coroutine** is a compiler-transformed function;
- an **awaiter** defines one suspension operation;
- a **runtime** or executor decides when and where suspended work resumes.

C++20 supplies the first two mechanisms. It does **not** supply a standard `task<T>`, event loop, timer, or async-I/O runtime.

## Learning path

1. [What are coroutines?](01_what_are_coroutines.md)
2. [Why coroutines?](02_why_coroutines.md)
3. [Lifecycle](03_coroutine_lifecycle.md)
4. [Coroutine frame and lifetime](04_coroutine_frame.md)
5. [Suspension and resumption](05_suspension_resumption.md)
6. [`co_await` mechanics](06_coawait_mechanics.md)
7. [`co_yield` and generators](07_coyield_generators.md)
8. [Creating awaitables](08_creating_awaitables.md)
9. [Creating a `task<T>`](09_creating_coroutine_types.md)
10. [Handles and ownership](10_coroutine_handle.md)
11. [Exceptions and cancellation](11_exception_handling.md)
12. [Practical patterns](12_practical_patterns.md)

Read the lessons in order through Lesson 6. After that, Lessons 7–12 can be read according to your use case.

## Build the checked examples

The complete examples in [`examples/`](examples/) are compiled as part of one CMake project:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Or compile one directly:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic examples/manual_task.cpp -o manual_task
```

Code labelled **pseudocode** is explanatory and is not intended to compile. Short fragments may depend on types introduced immediately above them. The files in `examples/` are the runnable reference implementations.

## Non-negotiable safety rules

1. A `std::coroutine_handle` is non-owning. Decide exactly who calls `destroy()`.
2. Never resume a null handle, a running coroutine, or a coroutine at final suspend.
3. Never destroy a running coroutine; `destroy()` requires suspension.
4. If `final_suspend()` does not suspend, any returned handle becomes dangling when the coroutine finishes.
5. By-reference parameters, including the implicit `this` pointer, can dangle while a coroutine is suspended.
6. Publishing a handle from `await_suspend()` may allow another thread to resume and destroy the coroutine before `await_suspend()` returns. Do not access the awaiter or frame after publication without synchronization.
7. Destroying an owner while a scheduler, callback, or OS operation still holds its handle is a use-after-free bug.

## Quick reference

| Term | Meaning |
|---|---|
| coroutine frame/state | compiler-managed storage for promise, parameter copies, live locals, awaiters, and bookkeeping |
| promise object | per-coroutine customization object selected through `std::coroutine_traits` |
| coroutine return object | the ordinary object returned by `promise.get_return_object()` |
| awaitable | operand accepted by `co_await`, possibly after `await_transform` |
| awaiter | object with `await_ready`, `await_suspend`, and `await_resume` |
| handle | small, copyable, non-owning reference to a coroutine state |
| continuation | coroutine to resume when another coroutine completes |

## Scope

The tutorial targets C++20 and notes later-library facilities only when useful. Production code should normally use a mature coroutine library rather than inventing a task type for each application. The implementations here are educational: they make ownership and control transfer visible, but intentionally omit production concerns such as allocator integration, cancellation propagation, tracing, multi-await protection, and scheduler affinity.

See [CORRECTNESS_AUDIT.md](CORRECTNESS_AUDIT.md) for the audit findings and the changes made.

## AI Assistance Disclosure

This tutorial reflects my own learning goals, thoughts, and ideas about C++ coroutines. I used AI/LLM tools to help generate and reshape drafts, organize ideas, refine explanations, expand coverage, and review the material.

The material is intended for educational purposes. Code examples and technical explanations were reviewed and tested, but readers should verify important details against the C++ standard and the documentation of any libraries they use.
