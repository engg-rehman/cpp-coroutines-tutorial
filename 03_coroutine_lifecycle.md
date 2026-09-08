---
title: "C++ Coroutine Lifecycle Explained"
description: "Trace a C++20 coroutine from state creation and initial_suspend through execution, final_suspend, exceptions, and frame destruction."
keywords:
  - C++ coroutine lifecycle
  - initial_suspend
  - final_suspend
  - coroutine frame destruction
  - promise_type lifecycle
seo:
  type: TechArticle
---

# Lesson 3: Coroutine Lifecycle

The standard specifies operations and control transfers, not a formal list of lifecycle “states.” A useful mental model is:

```text
create state
  -> obtain return object
  -> initial await
  -> run / suspend / resume (zero or more times)
  -> store result or exception
  -> final await
  -> destroy state
```

## 1. Invocation and state creation

Calling a coroutine first creates its coroutine state. Conceptually, the implementation:

1. obtains storage, unless allocation is elided;
2. creates a copy for every parameter;
3. constructs the promise object;
4. calls `promise.get_return_object()`;
5. evaluates `co_await promise.initial_suspend()`.

These steps precede the user's function body. The caller receives the return object either when initial suspension occurs or when eager execution later suspends/completes.

The return object and promise are different objects. The return object is usually a small owner containing a handle; the promise lives in the coroutine state.

## 2. Initial suspend: lazy or eager start

```cpp
std::suspend_always initial_suspend() noexcept; // lazy
std::suspend_never  initial_suspend() noexcept; // eager
```

- A lazy coroutine returns to its caller before running user code. A consumer or scheduler must resume it.
- An eager coroutine starts inside the call and returns only when it first suspends or completes.

Neither policy is universally “most common.” Generators and many composable task types are lazy; event handlers and some task types are eager.

## 3. Running and ordinary suspension

While running, a coroutine is executing an ordinary instruction stream. At a `co_await`, it may suspend. If it does, the coroutine state retains the resume location and every object whose lifetime crosses that point.

Later, some code may call `resume()` on a handle. Resumption first completes the suspended await-expression by evaluating `await_resume()`, then continues the body.

Do not call `resume()` merely because you have a handle. Its preconditions require a suspended coroutine that is not at final suspend. Resuming a running or completed coroutine is undefined behavior.

## 4. Normal completion

```cpp
co_return value; // calls promise.return_value(value)
co_return;       // calls promise.return_void()
```

After the appropriate promise method finishes, local variables are destroyed and execution proceeds to the final await. An ordinary `return` statement is forbidden in a coroutine.

## 5. Exceptional completion

An exception escaping the user body is caught by the generated machinery and passed to:

```cpp
promise.unhandled_exception();
```

A result-bearing task normally stores `std::current_exception()` in the promise. Its awaiter rethrows from `await_resume()`, making the exception appear at the awaiting coroutine's `co_await` expression.

The exception does not automatically cross the coroutine boundary. A promise that swallows it has chosen to lose it; a promise that calls `std::terminate()` has chosen a fire-and-forget policy.

## 6. Final suspend

Every completed coroutine evaluates:

```cpp
co_await promise.final_suspend();
```

The final await-expression must not be potentially throwing, so `final_suspend()` and its awaiter operations are normally `noexcept`. Common policies:

- `suspend_always`: retain the completed state until its owner destroys it;
- a custom final awaiter: resume a stored continuation, usually by returning its handle;
- `suspend_never`: do not retain the state; it is destroyed as completion finishes.

The last policy is dangerous if a return object still stores a handle: that handle becomes dangling as soon as the coroutine completes. Owning task examples in this tutorial therefore suspend at final suspend.

A coroutine suspended at final suspend is done. It may be inspected or destroyed, but never resumed.

## 7. Destruction

Calling `handle.destroy()` on a suspended coroutine:

1. destroys objects whose lifetimes remain in the coroutine state;
2. destroys the promise;
3. destroys parameter copies;
4. releases the coroutine-state storage when needed.

Destroying a suspended coroutine before completion is allowed and represents abandonment. The owner must first ensure no callback, queue, timer, or other thread can later resume the old handle.

Destroying a running coroutine is undefined behavior.

## The ownership pattern

An owning return object should be move-only:

```cpp
class owner {
    std::coroutine_handle<promise_type> handle_{};

public:
    owner(const owner&) = delete;
    owner& operator=(const owner&) = delete;

    owner(owner&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    ~owner() {
        if (handle_) {
            handle_.destroy();
        }
    }
};
```

If an owner were copyable, two destructors could destroy the same state. A defaulted move constructor is also insufficient for `coroutine_handle`, because copying a handle does not clear the source.

## Failure during creation

There are several distinct cases:

- If ordinary coroutine-state allocation throws, the exception reaches the caller.
- A promise may define `get_return_object_on_allocation_failure()`. That selects a non-throwing allocation path and lets the call return a sentinel object instead.
- If constructing a parameter copy or the promise throws, already-created objects are destroyed, storage is released, and the exception reaches the caller.
- Failure while entering the initial await, before its `await_resume()` begins, also reaches the caller rather than being sent to `unhandled_exception()`.

These creation failures occur before there is a normally usable coroutine return object.

## Trace the checked example

In [`examples/manual_task.cpp`](examples/manual_task.cpp):

1. the call creates an eager coroutine;
2. it prints `coroutine: start`;
3. `suspend_always` suspends it;
4. `main` calls the owner's safe `resume()` wrapper;
5. the coroutine finishes and suspends at final suspend;
6. the owner's destructor calls `destroy()`.

Add logging to `get_return_object`, `initial_suspend`, `return_void`, `final_suspend`, and the promise destructor to see the exact order.

Next: [Lesson 4 — The Coroutine Frame](04_coroutine_frame.md)
