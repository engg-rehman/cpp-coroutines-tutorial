---
title: "Lesson 10: Repair Ownership at the Ready Queue"
description: "Distinguish handles from owners, move a coroutine owner, and unregister queued resumptions before frame destruction."
keywords:
  - std::coroutine_handle
  - C++ coroutine ownership
  - coroutine destroy
  - coroutine resume
  - dangling coroutine handle
seo:
  type: TechArticle
---

# Lesson 10: Destroy a job without leaving a stale resume behind

The event loop stores raw handles. The task owns frames. Those facts are individually simple, but together they create a lifetime requirement. We will make owner destruction remove a pending queue entry.

## 1. Let the compiler stop a second owner

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/10/01_copy_owner.cpp -o scratch/ownership
```

[01_copy_owner.cpp](labs/10/01_copy_owner.cpp) attempts `auto second = first;`. Expected: use of a deleted copy constructor.

Replace that line with `auto second = std::move(first);` in a local copy. The move constructor transfers the handle and clears `first`. One destructor now owns the destroy operation. A raw `coroutine_handle`, however, remains freely copyable. It copies a capability, not a frame or a reference count.

## 2. Copy the handle and move the owner

Build [02_handles.cpp](labs/10/02_handles.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/10/02_handles.cpp -o scratch/ownership
./scratch/ownership
```

Expected:

```text
same state: 1
body
done: 1
```

The experiment obtains a typed handle from our inspection hook, converts it to `coroutine_handle<>`, round-trips its opaque address, then moves the owner and resumes through the borrowed erased handle. Moving an owner did not relocate or clone the frame.

A typed handle offers `promise()` access. An erased handle still supports resumption, completion inspection, destruction, and opaque identity, but cannot expose an unknown promise type. `from_promise` is appropriate inside our return-object hook; `from_address` is only appropriate for an address originally obtained under a valid handle protocol. An arbitrary pointer is not a valid coroutine handle.

The example also resumes `std::noop_coroutine()`, which safely does nothing. Ordinary null handles do not have this behavior.

## 3. Find the stale entry before running the broken design

With Lesson 8's raw queue, this is an **unsafe design fragment**:

```cpp
event_loop loop;
{
    auto owner = work(loop);
    owner.start(); // Enqueues its handle.
}                 // Destroys the frame.
loop.run();       // Would resume a dangling handle.
```

Do not use a crash as the test. The ownership proof already fails: the queue can outlive the frame. `if (h && !h.done())` cannot repair it; `done()` itself requires a valid suspended coroutine.

We can require every caller to drain before destruction, as in Lesson 8. For this lesson, implement a different contract: a scheduling awaiter unregisters itself when destroyed while queued.

## 4. Give the awaiter a registration to remove

[The revised event_loop.hpp](labs/10/event_loop.hpp) keeps an additional member in its awaiter:

```cpp
std::coroutine_handle<> queued_{};
```

Its suspend method records the handle and inserts it. Its resume method clears the registration:

```cpp
void await_suspend(std::coroutine_handle<> h) {
    queued_ = h;
    loop_.ready_.push_back(h);
}
void await_resume() noexcept { queued_ = {}; }
~schedule_awaiter() { if (queued_) loop_.remove(queued_); }
```

`loop.remove(h)` uses `std::erase` on the queue. This linear removal is acceptable for a small teaching loop. The awaiter is noncopyable so a copied registration cannot unexpectedly unregister another live await.

If insertion throws, await failure unwinds the awaiter and removal finds nothing. If the owner is destroyed while queued, destruction of the frame destroys this awaiter and removes the entry. If the loop runs it normally, the loop pops before resuming and `await_resume` clears the registration.

The loop must outlive the awaiter. All queue access and owner destruction happen on one thread, so removal cannot race with another thread resuming the handle. A multithreaded queue requires more than this patch: synchronization plus a protocol for already-dequeued, in-flight work.

## 5. Run abandonment and completion back to back

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/10/03_unregister.cpp -o scratch/ownership
./scratch/ownership
```

[03_unregister.cpp](labs/10/03_unregister.cpp) first destroys a waiting job, then completes a second one:

```text
cleanup
queue empty: 1
finished
cleanup
```

The abandoned job prints only its local destructor, and its queue entry is gone. The completed job prints `finished`, then destroys its local through ordinary completion. Both frames are eventually destroyed exactly once by their owners.

**Your experiment:** add a second scheduling point before `finished`. Run one queue turn, then destroy the owner. Expect cleanup and an empty queue again. The registration belongs to the currently suspended awaiter, not permanently to a function name.

## 6. Apply the preconditions to every operation you used

| Operation | What our program must establish first |
|---|---|
| `resume()` | Live, suspended coroutine that is not at final suspend |
| `done()` | Live suspended coroutine; reports final suspension |
| `destroy()` | Live suspended coroutine; no later external resume can use it |
| `promise()` | Correct typed handle to a live state |
| `address()` round trip | Opaque identity remains tied to a live state |

A null handle is not suitable for `resume`, `done`, `destroy`, or promise access. A non-null handle may still dangle. Concurrent resume/destroy is not repaired by checking a flag. Do not expose raw handles as an unrestricted public control API just because this lesson uses an inspection hook.

## 7. Compare the other ownership choices against this experiment

Our owner retains a final-suspended frame and destroys it with RAII. A self-destroying coroutine could instead use `final_suspend = suspend_never`, as Lesson 1 did. It must not leave an owner or observer that later touches its handle. If it can wait externally, it still needs an error sink, a lifetime while waiting, and a shutdown policy. “Detached” does not remove those obligations.

Lesson 8 used separately owned callback state. The callback's `shared_ptr` kept results and completion flags alive; it did not own the coroutine frame. In a concurrent implementation, that state may also coordinate cancellation, callback quiescence, and permission to resume. The relationship between shared state and frame lifetime must be explicit.

A child task's continuation also does not own its parent. In our chain, the parent owns the child temporary until completion, and the child refers back only while nested inside that lifetime. Detaching either side invalidates that simple argument.

## Use an identity that survives address reuse

For a debugging experiment, add an incrementing operation ID to the promise and log it at registration, dequeue, final suspend, and destruction. A handle address can help inspect one live state, but an allocator may reuse that address for another job after destruction.

A stuck job can now be investigated by locating its owner, its current awaiter, and the agent responsible for the next resume. A stale resume can be investigated by comparing registration and destruction events. AddressSanitizer helps catch stale-frame access; ThreadSanitizer is relevant once a real concurrent protocol exists.

```bash
python3 tools/check_labs.py 10
```

Next: [Lesson 11 — Deliver errors and request cancellation](11_exception_handling.md).
