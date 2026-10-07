---
title: "Lesson 9: Build task<T> by Losing and Restoring a Continuation"
description: "Add result storage, repair a non-awaitable task, and reconnect child completion to its parent through symmetric transfer."
keywords:
  - C++ task coroutine
  - task<T> C++20
  - promise_type implementation
  - coroutine continuation
  - lazy task C++
seo:
  type: TechArticle
---

# Lesson 9: Get a result from another coroutine

A generator yields many values. Now we need a child operation that produces one result, and a parent that continues with that result. We will first build result storage, then discover that returning a result and resuming a waiting parent are separate requirements.

## 1. Ask the current promise to return a number

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/09/01_missing_return_value.cpp -o scratch/task
```

[01_missing_return_value.cpp](labs/09/01_missing_return_value.cpp) tries `manual_job answer() { co_return 42; }`.

Expected: missing `return_value`. Lesson 1's `return_void` cannot handle a value. Do not add both hooks to the same promise: finding both names makes the coroutine ill-formed.

## 2. Store one eventual result

Create a template `task<T>` using the same owning-handle pattern as `manual_job`. Keep lazy initial suspension and retained final suspension. Replace the completion hook with:

```cpp
std::optional<T> value;
std::exception_ptr error;

template<class U>
void return_value(U&& result) {
    value.emplace(std::forward<U>(result));
}
void unhandled_exception() noexcept {
    error = std::current_exception();
}
```

The optional avoids default-constructing `T`. Forwarding permits constructing `T` from suitable lvalues and rvalues. Construction can throw, so `return_value` is not unconditionally `noexcept`. A production interface would also constrain acceptable conversions for better diagnostics.

Add a temporary synchronous driver, `run()`: resume once, require final suspension, rethrow any stored exception, then move the result out. The complete intermediate type is [result_task.hpp](labs/09/result_task.hpp).

Compile [02_value.cpp](labs/09/02_value.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/09/02_value.cpp -o scratch/task
./scratch/task
```

Expected: `42`.

The call returns a task owner before the lazy body starts. `co_return 42` later places the integer in the promise. `run()` finally delivers that integer to `main`. There are two different returns in that story: the coroutine return object and the eventual result.

## 3. Try composing two tasks

[03_cannot_await.cpp](labs/09/03_cannot_await.cpp) adds:

```cpp
task<int> parent() {
    co_return (co_await answer()) + 1;
}
```

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/09/03_cannot_await.cpp -o scratch/task
```

Expected: missing `await_ready`. Our task can own a coroutine and extract a result, but it is not yet awaitable. The compiler does not infer how two owning wrappers should connect.

## 4. Add an awaiter—and leave out one essential line

An awaiter can borrow the child handle. It checks whether the child is already done, returns the child handle from `await_suspend`, and reads the promise in `await_resume`:

```cpp
std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) const noexcept {
    // Something needs to happen with parent here.
    return handle;
}
```

Returning the child transfers control to it. But how does finishing the child resume the parent? To investigate that missing edge, [unlinked_task.hpp](labs/09/unlinked_task.hpp) gives each promise a `continuation` initialized to `std::noop_coroutine()`. Its final awaiter returns that stored continuation. The deliberately broken awaiter never updates it.

Build [04_lost_parent.cpp](labs/09/04_lost_parent.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/09/04_lost_parent.cpp -o scratch/task
./scratch/task
```

Expected:

```text
child finished
task needs an external scheduler
```

The child completed, but the parent never reached its `+ 1`. Our bounded `run()` resumes once and detects that the root remains suspended, so this experiment reports the fault instead of hanging. No external queue holds the parent, and destroying the root safely destroys its completed child temporary.

Do not “fix” this by repeatedly resuming the parent from `main`. A parent suspended on a child must continue when that child has a result, not whenever a polling loop guesses it might.

## 5. Connect both transfer directions

Restore the missing line:

```cpp
handle.promise().continuation = parent;
return handle;
```

The child's final awaiter is:

```cpp
struct final_awaiter {
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(handle_type h) const noexcept {
        return h.promise().continuation;
    }
    void await_resume() const noexcept {}
};
```

The complete repaired type is [task.hpp](labs/09/task.hpp). Read `operator co_await`, the awaiter, and the final awaiter together. The parent first transfers to the child. The child's final await transfers back. The child remains at final suspension while its result is consumed.

Build [05_chain.cpp](labs/09/05_chain.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/09/05_chain.cpp -o scratch/task
./scratch/task
```

Expected:

```text
child
parent
43
void task
task already started
```

Now draw the control path beside the output:

```text
main resumes parent
parent saves itself as child's continuation and transfers to child
child stores 42, reaches final suspend, transfers to parent
parent's await_resume reads 42; parent stores 43
parent's final await returns its default noop continuation
main receives control and reads 43
```

Returning handles implements symmetric transfer. We did not recursively call `resume()` in both awaiters, and did not need to put either transfer through a ready queue.

## 6. Account for the child temporary

In `int n = co_await answer();`, the temporary returned by `answer()` remains alive through the await-expression, including suspension. Our awaiter borrows its handle. After the await-expression finishes, the temporary destroys the completed child frame.

The parent frame owns that temporary; the child's continuation is only a non-owning reference back to the parent. This nesting is why both frames remain valid. If you await a named task, keep it alive and use `co_await std::move(child)`. Moving the expression enables our rvalue-qualified await operator; the operator itself borrows the handle rather than transferring it to a second owner.

Our type rejects awaiting an lvalue and records whether execution has already been claimed. A second `run`, `start`, or await fails instead of silently overwriting a continuation. `result()` also rejects repeated consumption. These are single-thread guards, not atomic protection against concurrent calls. The task is single-consumer, not a shared future.

Moving the owner clears its source handle and transfers these flags. Move assignment is intentionally absent; overwriting a running operation needs a separate abandonment contract.

## 7. Make `task<void>` work without declaring both completion hooks

The saved implementation factors result storage into a helper:

```cpp
template<class T> struct result_storage {
    std::optional<T> value;
    template<class U> void return_value(U&& v) {
        value.emplace(std::forward<U>(v));
    }
    T take();
};
template<> struct result_storage<void> {
    void return_void() const noexcept {}
    void take() const noexcept {}
};
```

The promise inherits the appropriate specialization. Each instantiated promise has exactly one return hook. The rest of the ownership, error, and continuation machinery is shared. That is why `notify()` in the checkpoint can use `co_return;` and print `void task`.

## 8. Stop treating a one-resume driver as a scheduler

The final type exposes `start()`, `done()`, and `result()` for the later event-loop examples. `run()` is a convenience only for chains that finish through synchronous execution and symmetric transfers.

**Your experiment:** add `co_await std::suspend_always{};` inside the child in a local copy. Expect `task needs an external scheduler` again, now for an actual unserved pause. The driver is deliberately unable to invent the event that resumes it. If an external queue is involved, retain the root, drive that queue, then read `result()`—do not let a failed `run()` unwind an owner while stale registrations remain.

A general `sync_wait` needs an event/completion protocol and must not block the same executor required to finish the task. Our task has no implicit executor, stop-token propagation, shared results, detachment policy, allocator integration, or arbitrary-graph stack guarantee. Later lessons add explicit scheduling and cancellation at application boundaries.

```bash
python3 tools/check_labs.py 09
```

Next: [Lesson 10 — Make queued handles safe to abandon](10_coroutine_handle.md).
