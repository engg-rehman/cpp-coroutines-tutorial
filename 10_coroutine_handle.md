---
title: "std::coroutine_handle and Safe Coroutine Ownership"
description: "Understand typed and erased coroutine handles, resume and destroy preconditions, unique ownership, self-destruction, continuations, and abandonment."
keywords:
  - std::coroutine_handle
  - C++ coroutine ownership
  - coroutine destroy
  - coroutine resume
  - dangling coroutine handle
seo:
  type: TechArticle
---

# Lesson 10: `std::coroutine_handle` and Ownership

## A handle is a non-owning capability

`std::coroutine_handle<Promise>` refers to a coroutine state and can:

- resume it;
- destroy it;
- test whether it is at final suspend;
- access its promise when the handle is typed;
- expose an opaque address for low-level interoperation.

Copying a handle copies only that capability. It does not copy the coroutine, increment a reference count, or extend state lifetime.

## Typed and erased handles

```cpp
using typed = std::coroutine_handle<my_promise>;

typed h = /* ... */;
std::coroutine_handle<> erased = h;

my_promise& promise = h.promise(); // typed only
erased.resume();                   // type-erased control is still possible
```

A typed handle knows how to expose the promise. An erased handle is useful in scheduler queues that need only resume operations.

The internal representation is implementation-defined. Treat `address()` as an opaque identity suitable for round-tripping with `from_address()` only under a proven protocol. It is not portably a pointer to the promise or the first byte of a known frame struct.

## Handle construction

Promise code commonly uses:

```cpp
handle_type::from_promise(*this)
```

Low-level systems may use:

```cpp
void* address = handle.address();
auto erased = std::coroutine_handle<>::from_address(address);
```

An address remains usable only while the referred coroutine state exists. Converting arbitrary pointers or stale addresses into handles does not validate them.

`std::noop_coroutine()` returns a special handle that is always safe to resume and does nothing. It is useful as a continuation sentinel.

## Preconditions matter

For an ordinary handle `h`:

```cpp
if (h && !h.done()) {
    h.resume();
}
```

This check is appropriate only when external synchronization already guarantees that `h` refers to a suspended live coroutine. It does not make concurrent access safe.

Key rules:

- a default/null handle cannot be resumed, destroyed, queried with `done()`, or used for promise access;
- `resume()` requires suspension and forbids final suspend;
- `destroy()` requires suspension;
- `done()` is intended for a suspended coroutine and reports final suspension;
- `promise()` requires a typed handle referring to a live coroutine;
- no two agents may race to resume/destroy the same state.

Checking `!h.done()` cannot detect a dangling handle. Dereferencing a stale capability is already invalid.

## Ownership model 1: unique RAII owner

The examples use a move-only wrapper:

```cpp
~task() {
    if (handle_) {
        handle_.destroy();
    }
}
```

This pairs naturally with a final suspend that retains the completed state. Scheduler queues and continuations hold non-owning copies, so the system must prevent the owner from destroying while any can still resume.

Move construction must clear the source:

```cpp
task(task&& other) noexcept
    : handle_(std::exchange(other.handle_, {})) {}
```

A defaulted move would merely copy the handle and create two apparent owners.

## Ownership model 2: self-destroying coroutine

A fire-and-forget coroutine may use `final_suspend = suspend_never`, allowing completion to destroy its own state. Such a type must not leave an owning wrapper or external observer with a handle that it later touches.

This model also needs an exception policy—often logging followed by termination or controlled reporting—and a shutdown policy for operations still suspended before final completion.

Self-destruction is a specialized design, not a shortcut for avoiding RAII.

## Ownership model 3: separately owned operation state

Some async libraries keep result, cancellation, and continuation data in a reference-counted operation state. The coroutine frame may still have unique ownership, while callbacks retain the separate state. Completion and cancellation jointly decide when the handle may be resumed or destroyed.

Do not infer that a `shared_ptr` to operation state automatically owns the coroutine frame. That relationship must be explicit.

## Abandonment

Destroying a suspended, incomplete coroutine abandons it. Local destructors run, but the language does not automatically cancel external I/O or remove queued callbacks.

Before destruction, the abstraction must establish one of:

- no external agent knows the handle;
- the external operation has been cancelled and quiesced;
- external callbacks consult state that prevents stale resumption;
- ownership is transferred so destruction cannot yet occur.

This is why a destructor that blindly calls `destroy()` can still be wrong even though `destroy()` itself is valid for a suspended state.

## Continuations are normally non-owning

In the tutorial task, a child promise stores the parent handle as a continuation. The parent owns the child task temporary; neither handle itself establishes ownership.

Structured nesting makes the lifetime valid:

```text
parent state owns child task wrapper
child promise temporarily refers to parent continuation
child completes and resumes parent
parent finishes await-expression and destroys child wrapper
```

Detaching either side breaks this simple proof and requires a stronger scheme.

## Debugging handles

Useful diagnostics include:

- a unique operation ID stored in the promise;
- current phase (`created`, `scheduled`, `running`, `final`, `destroyed`) in debug builds;
- logging every scheduling edge with source and destination IDs;
- assertions against second await, duplicate resume, and destruction while registered;
- AddressSanitizer for stale handles and ThreadSanitizer for resume races.

Never rely on the raw address alone as a stable identity after destruction; allocators can reuse it.

Next: [Lesson 11 — Exceptions and Cancellation](11_exception_handling.md)
