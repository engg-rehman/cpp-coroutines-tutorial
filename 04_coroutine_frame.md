---
title: "C++ Coroutine Frame, Storage, and Lifetime"
description: "Learn what a C++ coroutine frame contains, when allocation can be elided, and how parameters, locals, this, and lambda captures can dangle."
keywords:
  - C++ coroutine frame
  - coroutine allocation elision
  - coroutine lifetime
  - dangling reference coroutine
  - coroutine memory
seo:
  type: TechArticle
---

# Lesson 4: Coroutine State, Storage, and Lifetime

## What the coroutine state contains

The implementation creates a coroutine state containing whatever is needed to suspend and resume correctly. It normally includes:

- the promise object;
- copies of parameters;
- local variables and temporaries whose lifetimes cross suspension;
- the current suspension/resume state;
- implementation-specific resume and destroy information.

“Coroutine frame” is the common informal name for this state and its storage.

There is no portable C++ struct layout for a frame. The promise is not guaranteed to be first, the resume state need not be an integer, and fields need not follow source declaration order. Do not inspect offsets or use `sizeof(promise_type)` as the frame size.

## Dynamic allocation is not guaranteed

The abstract coroutine machinery can request dynamic storage through an allocation function. However, the compiler may elide that allocation and embed the state in the caller's storage when it can prove that:

- the coroutine state's lifetime is strictly nested within the caller's lifetime; and
- the required size is known at the call site.

This is sometimes called the **coroutine allocation elision optimization**. It can occur even if promise-specific `operator new` has observable behavior, so allocation logging is a useful experiment, not a semantic guarantee.

Avoid saying that every coroutine is heap allocated. Also avoid saying that suspension “returns the function's stack.” Suspension transfers control; the compiler-managed state outlives the activation that initiated the call.

## Which locals occupy the state?

Consider:

```cpp
task<int> example() {
    int before = expensive_calculation();
    consume(before);

    int across = 7;
    co_await operation();
    co_return across;
}
```

`across` must survive the suspension. `before` does not need to, because its last use precedes suspension. Optimizers can shorten lifetimes, reuse storage, inline awaiters, and remove values entirely.

Scope objects still follow normal RAII rules. If a lock guard's scope crosses `co_await`, the lock remains held while the coroutine is suspended—usually a serious bug.

```cpp
std::unique_lock lock(mutex);
co_await network_read(); // generally wrong: suspension duration is unbounded
```

Finish the protected operation and release the lock before awaiting.

## Parameter-copy rules

At invocation, the coroutine creates a frame-resident copy corresponding to every parameter:

- for a non-reference parameter, the copy is initialized from an xvalue referring to the original parameter, so a move may occur;
- for a reference parameter, the copy is still a reference bound to the same object.

Therefore this is potentially unsafe:

```cpp
task<void> print_later(const std::string& text) {
    co_await schedule();
    std::cout << text; // text may refer to a destroyed object
}

auto work = print_later(std::string{"temporary"});
```

Passing by value makes the string object part of the coroutine state:

```cpp
task<void> print_later(std::string text) {
    co_await schedule();
    std::cout << text;
}
```

“Always pass by value” is too broad: a reference is valid if the referent is guaranteed to outlive the coroutine. The important requirement is an explicit lifetime relationship.

## The implicit `this` trap

A non-static member coroutine retains the implicit object parameter as a pointer-like reference; it does not copy the object.

```cpp
struct session {
    task<void> run() {
        co_await read_request();
        use_member_data(); // *this must still be alive
    }
};
```

Destroying `session` while `run()` is suspended makes later member access invalid. Common solutions are structured ownership, a task owned by the object, or deliberately retaining shared ownership. Blindly capturing a `shared_ptr` can also create cycles, so document the design.

## Coroutine lambdas and captures

Immediately invoking a capturing coroutine lambda is a notorious lifetime trap:

```cpp
auto task = [value = std::string{"hello"}]() -> task<void> {
    co_await schedule();
    std::cout << value;
}(); // closure temporary can die while the coroutine keeps a `this` pointer
```

The captures belong to the lambda closure, not automatically to the coroutine state. Prefer a named/free coroutine that takes values as parameters:

```cpp
task<void> print_after_schedule(std::string value) {
    co_await schedule();
    std::cout << value;
}
```

Or ensure the closure itself outlives the coroutine.

## Custom allocation

The promise type can provide allocation functions used for the coroutine state:

```cpp
struct promise_type {
    static void* operator new(std::size_t bytes) {
        return ::operator new(bytes);
    }

    static void operator delete(void* pointer, std::size_t bytes) noexcept {
        ::operator delete(pointer, bytes);
    }
};
```

Placement forms can receive leading coroutine arguments when their signatures match, which enables allocator-aware designs. Such designs must align deallocation, promise construction failure, and argument lifetimes correctly; use an established implementation as a reference.

If the promise declares:

```cpp
static task get_return_object_on_allocation_failure() noexcept;
```

the coroutine uses a non-throwing allocation path and returns that object on failure. This hook changes creation semantics and should be tested explicitly.

## Ownership versus storage

These are independent questions:

- **Where is the state stored?** Chosen by the compiler/allocation protocol.
- **Who controls its lifetime?** Chosen by the coroutine abstraction.

An RAII return object can own a handle regardless of whether allocation was ultimately elided. Calling `handle.destroy()` invokes the correct generated destroy path; user code does not directly free frame memory.

## Measuring instead of guessing

To investigate a real implementation:

1. build optimized and unoptimized variants;
2. log promise allocation calls, while remembering they may be elided;
3. inspect compiler coroutine lowering or assembly;
4. benchmark allocation rate, peak outstanding states, scheduler operations, and cache behavior;
5. repeat after changing which locals cross suspension.

Frame sizes such as “100–500 bytes” are anecdotes, not portable facts.

Next: [Lesson 5 — Suspension and Resumption](05_suspension_resumption.md)
