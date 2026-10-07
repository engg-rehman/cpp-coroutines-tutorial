---
title: "Lesson 2: Pause, Resume, and Keep Local State"
description: "Discover why coroutines help by evolving a restarting counter into two interleaved resumable functions."
keywords:
  - why use C++ coroutines
  - coroutines vs threads
  - coroutines vs callbacks
  - std::future
  - asynchronous C++
seo:
  type: TechArticle
---

# Lesson 2: Make a function remember where it stopped

We can compile `co_return`. Now give the program a useful requirement: produce `1`, let `main` do something else, then continue and produce `2` using the same local counter.

## 1. Watch an ordinary function restart

Build [01_repeat.cpp](labs/02/01_repeat.cpp):

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/02/01_repeat.cpp -o scratch/count
./scratch/count
```

Its complete body is:

```cpp
void count() { int n = 0; std::cout << ++n << '\n'; }
```

Calling it twice prints:

```text
1
1
```

You could move `n` into an object and maintain an explicit state machine. That is a good solution for a small counter. Our experiment is to keep both the local state and the point of continuation inside the function. This same need appears in a parser waiting for input or a sequence producing one item at a time.

## 2. Put a pause between the two increments

Start with Lesson 1's program in your editor. Change the coroutine body to:

```cpp
manual_job count() {
    int n = 0;
    std::cout << ++n << '\n';
    co_await std::suspend_always{};
    std::cout << ++n << '\n';
}
```

We are renaming the return type `manual_job` to describe how we intend to drive it. `co_await std::suspend_always{}` is an unconditional pause. It does not arrange a future resume. If we only added that line to Lesson 1, execution would stop with no way to reach it again. Do not run that incomplete ownership design; first add the handle below.

A **handle** is the capability the caller will use to reach the saved coroutine state. A **frame** is the retained state itself: the promise, parameter copies, needed locals, and resume bookkeeping. No portable frame layout is exposed.

## 3. Give the return object a handle

Inside the return type, before the promise definition, declare:

```cpp
struct promise_type;
using handle_type = std::coroutine_handle<promise_type>;
```

Replace the old empty `get_return_object()` with:

```cpp
manual_job get_return_object() noexcept {
    return manual_job{handle_type::from_promise(*this)};
}
```

`*this` here is the promise inside the coroutine state. `from_promise` produces a handle to that state. The returned `manual_job` will store the handle.

Add storage and a constructor to the outer class:

```cpp
explicit manual_job(handle_type h) noexcept : handle_(h) {}
// In the private section:
handle_type handle_;
```

Now decide who destroys the state. We choose the returned object. Add these operations to its public section and include `<utility>`:

```cpp
manual_job(const manual_job&) = delete;
manual_job& operator=(const manual_job&) = delete;
manual_job(manual_job&& other) noexcept
    : handle_(std::exchange(other.handle_, {})) {}
~manual_job() { if (handle_) handle_.destroy(); }
```

Copying would create two owners that both destroy the same state. Moving transfers the handle and clears the source. A defaulted move would merely copy this handle. We do not need move assignment in this workshop type.

**Change `final_suspend()` to return `std::suspend_always`.** Otherwise completion frees the state while our owner still holds a handle, and the owner's destructor later uses a dangling handle.

## 4. Try lazy start—and forget to start it

Also change `initial_suspend()` to return `std::suspend_always`. This makes the call create the coroutine and give its owner back before running the body.

Add two owner methods:

```cpp
bool resume() {
    if (!handle_ || handle_.done()) return false;
    handle_.resume();
    return !handle_.done();
}
bool done() const noexcept { return !handle_ || handle_.done(); }
```

The full reusable type is [manual_job.hpp](labs/02/manual_job.hpp). Compare your edits against it; its `native_handle()` method is an inspection hook used in Lesson 10. All operations here assume one thread and a live suspended coroutine. A `done()` check is not a concurrency or dangling-pointer check.

Compile and run the version that merely creates the owner:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/02/02_forgot_start.cpp -o scratch/count
./scratch/count
```

Output:

```text
main finished
```

Where are `1` and `2`? The body never started. The initial pause happened before the first line. The owner destroys that suspended state when it leaves scope. This failure to do useful work is a behavior problem, even though the compiler is satisfied.

## 5. Resume the same invocation twice

Use [03_resume.cpp](labs/02/03_resume.cpp), whose `main` adds:

```cpp
auto work = count();
work.resume();
std::cout << "main: other work\n";
work.resume();
std::cout << "done: " << work.done() << '\n';
```

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/02/03_resume.cpp -o scratch/count
./scratch/count
```

Predict the output first:

```text
1
main: other work
2
done: 1
```

The first resume passes initial suspension and reaches our explicit pause. The second continues that same invocation. `n` is still `1`, so incrementing produces `2`. Completion reaches final suspension; the owner can inspect `done()` and then destroy the retained frame.

Calling our guarded `resume()` again returns `false`. Calling a raw handle's `resume()` at final suspend would be undefined behavior.

## 6. Interleave two counters

Compile [04_interleave.cpp](labs/02/04_interleave.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/02/04_interleave.cpp -o scratch/count
./scratch/count
```

It owns two invocations and alternates `a.resume()` with `b.resume()`:

```text
A1
B1
A2
B2
```

Both counters make progress over the same interval. Only one executes at a time, on the thread running `main`. This is cooperative concurrency; there is no parallel execution. Suspension allows another piece of work to run. An asynchronous operation would additionally arrange to notify us when something becomes ready. Lesson 8 supplies that arrangement.

Try increasing the loop bound from two to three. Then count resumes: after the last yielded pause, one further resume is needed to leave the loop and reach final suspension. Printing the last number is not the same as completing.

## Apply what you observed

| If you implement this with… | Where does the counter's continuation state live? |
|---|---|
| An explicit state machine | Fields and a hand-maintained program counter |
| Callbacks | Captured/shared state plus the next callback |
| A blocking thread | Its live stack while it waits |
| This coroutine | Compiler-managed state retained between resumes |

Coroutines help when a flow repeatedly waits or produces values while preserving local variables, loops, and scopes. They do not speed up a CPU loop by themselves. Calling a blocking function in the body still blocks its executing thread. For a tiny synchronous counter, the explicit state machine may be simpler.

C++20 supplies the coroutine keywords, traits, handles, the two suspension helpers, and a noop handle. It supplies no standard `task<T>`, generator, timer, event loop, `when_all`, or automatic cancellation propagation. `std::future::get()` blocks; C++20 `std::future` is not automatically awaitable. Those missing pieces explain why we are building small types rather than expecting syntax alone to run an application.

**Your experiment:** put two `resume()` calls for A before either call for B. Expect `A1 A2 B1 B2`. Scheduling policy lives in the caller you just edited. Later, a runtime will make that decision. Multiple coroutine libraries may make different decisions and are not automatically interoperable.

```bash
python3 tools/check_labs.py 02
```

Next: [Lesson 3 — Trace creation, completion, and destruction](03_coroutine_lifecycle.md).
