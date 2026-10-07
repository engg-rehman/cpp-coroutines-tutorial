---
title: "Lesson 3: Trace the Coroutine Lifecycle"
description: "Instrument the coroutine you built and compare lazy start, normal completion, and abandonment."
keywords:
  - C++ coroutine lifecycle
  - initial_suspend
  - final_suspend
  - coroutine frame destruction
  - promise_type lifecycle
seo:
  type: TechArticle
---

# Lesson 3: Find out when things actually happen

We have an owner, a promise, a local counter, and two kinds of suspension. It is tempting to say “the function returns, so everything is destroyed.” Let's make that claim fail visibly.

## 1. Instrument the program before changing its behavior

Open [traced_job.hpp](labs/03/traced_job.hpp). It is Lesson 2's owner with a print in each promise hook, the promise constructor/destructor, and the owner's destroy path. Start by adding the same prints to your own copy.

[01_finish.cpp](labs/03/01_finish.cpp) adds a local object:

```cpp
struct guard {
    guard() { std::cout << "local: construct\n"; }
    ~guard() { std::cout << "local: destroy\n"; }
};
```

Its coroutine constructs `guard local`, prints `body: first`, pauses, prints `body: last`, and explicitly executes `co_return;`. Before running, write down where you expect the local destructor and promise destructor to appear.

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/03/01_finish.cpp -o scratch/lifecycle
./scratch/lifecycle
```

Expected output:

```text
main: call
promise: construct
promise: return object
promise: initial_suspend
main: resume 1
local: construct
body: first
main: resume 2
body: last
promise: return_void
local: destroy
promise: final_suspend
main: leave
owner: destroy frame
promise: destroy
```

## 2. Explain the two different lifetimes

The body local exists from its declaration until the body completes. It survives the ordinary pause. It is destroyed **before** final suspension.

The promise remains in the frame at final suspension. Only the owner's destructor destroys that completed frame. Thus “done” and “destroyed” are separate events. This distinction lets a future `task<int>` leave its result in the promise until the consumer reads it.

The actual creation sequence includes work before these prints: obtain state storage if required, construct parameter copies, then construct the promise. The promise constructor may accept arguments corresponding to the coroutine's parameters; otherwise the machinery uses the available default construction path. `get_return_object()` happens before initial suspension, which is why the caller receives an owner even when the body has not run.

The frame, promise, and return object are not interchangeable names. Locate all three in the code rather than memorizing definitions.

## 3. Remove the second resume

Build [02_abandon.cpp](labs/03/02_abandon.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/03/02_abandon.cpp -o scratch/lifecycle
./scratch/lifecycle
```

Everything through `body: first` is the same. The ending changes to:

```text
main: leave
owner: destroy frame
local: destroy
promise: destroy
```

There is no `body: last`, `return_void`, or `final_suspend` print. `destroy()` abandons the suspended body; it does not resume it to the end. It invokes cleanup for live objects. That is ordinary RAII applied to retained state.

This is safe here because no queue or callback holds a handle. Once an external system can resume us, destroying the frame also requires removing or disabling those registrations. We will implement that in Lesson 10.

## 4. Change initial suspension and predict the new trace

Copy both files together so the local include still resolves:

```bash
cp labs/03/traced_job.hpp scratch/traced_job.hpp
cp labs/03/01_finish.cpp scratch/lifecycle.cpp
```

In `scratch/traced_job.hpp`, change **only** `initial_suspend`'s return type from `std::suspend_always` to `std::suspend_never`. Compile `scratch/lifecycle.cpp` and run it.

Expected difference: `local: construct` and `body: first` move before `main: resume 1`. The body now starts inside the call. The first explicit resume completes it; the second guarded resume does nothing. This is eager start. Lazy start delayed the body until the caller explicitly resumed it.

Neither policy schedules work on another thread. The choice controls when user code starts relative to the call.

Do not make the same change to `final_suspend` while retaining this owner. A non-suspending final await would destroy the frame before the owner tries to use or destroy its handle. That variant has a lifetime bug; a successful compile cannot validate it.

## 5. Place results and exceptions on the same trace

At a valueless `co_return`, `return_void()` runs, live body locals are destroyed, and the coroutine reaches final suspension. At `co_return value`, `return_value(value)` replaces `return_void()`.

An exception escaping the body destroys affected locals and reaches `unhandled_exception()`. Our current promise terminates there. In Lesson 11, that method will store the exception, then allow final suspension and delivery to the consumer. Exceptions do not automatically appear at the original call site just because the function was called there.

There is one important boundary: failures during allocation, parameter copying, promise construction, `get_return_object`, or entry into the initial await before its `await_resume()` begins propagate through the original invocation. Already-created state is cleaned up as required. Exceptions from initial `await_resume()` are handled through the coroutine's exception machinery. Our initial/final hooks are nonthrowing to keep the ownership protocol simple.

If a custom `unhandled_exception()` itself throws, the language treats the coroutine as suspended at final suspend while that exception escapes. Our types avoid this policy: storing `exception_ptr` or terminating is easier to reason about.

## 6. Use this trace to diagnose a stuck coroutine

Draw the path from this experiment:

```text
call -> create state -> return object -> initial pause
resume -> local constructed -> ordinary pause
resume -> return_void -> local destroyed -> final pause
owner dies -> promise destroyed -> parameter copies destroyed -> storage released
```

The owner uses the generated destroy operation. It does not call `delete` on a guessed frame layout. Storage allocation may be optimized away, but the lifetime behavior still has to be correct.

**Your experiment:** put the `guard` in a nested scope that ends before the pause. Its destructor should move before control returns to `main` at the first pause. Then put it back and abandon the coroutine. Its destructor should run during owner destruction. Explain both results using scope, rather than whether a native stack is currently active.

**Checkpoint:** given a trace ending at initial suspension, ordinary suspension, or final suspension, identify whether the body local exists and who still owns the frame. Never resume at final suspend; never destroy a running coroutine.

```bash
python3 tools/check_labs.py 03
```

Next: [Lesson 4 — Discover what survives suspension](04_coroutine_frame.md).
