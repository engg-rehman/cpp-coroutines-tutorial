---
title: "Lesson 7: Build a Pull Generator"
description: "Start with a missing yield_value error and construct a finite generator with checked values, errors, and early cleanup."
keywords:
  - C++ co_yield
  - C++20 generator
  - generator coroutine
  - yield_value
  - lazy sequence C++
seo:
  type: TechArticle
---

# Lesson 7: Produce a sequence one value at a time

Our counter prints inside the producer. A consumer should instead be able to ask for the next number and decide what to do with it. We will build that pull interface.

## 1. Ask the existing type to yield

[01_missing_yield.cpp](labs/07/01_missing_yield.cpp) tries:

```cpp
manual_job numbers() { co_yield 1; }
```

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/07/01_missing_yield.cpp -o scratch/generator
```

Expected: missing `yield_value` in the promise.

You have seen this pattern with `return_void` and `return_value`: the keyword delegates behavior to the promise. Conceptually, `co_yield expression` awaits the result of `promise.yield_value(expression)`. It does not call `return_value` and it need not finish the coroutine.

## 2. Store the value before suspending

Copy the shape of `manual_job` into a `template<class T> class generator`. Keep lazy initial suspension, final suspension, move-only ownership, and `destroy()` in the destructor.

In its promise, add `<optional>` storage and the new hook:

```cpp
std::optional<T> current;
std::suspend_always yield_value(T value) {
    current.emplace(std::move(value));
    return {};
}
```

`optional` starts empty; `T` need not be default-constructible. `yield_value` stores a value and returns a pause, so the consumer can examine the value while the producer is suspended.

Notice the absence of `noexcept`. Constructing a general `T` in the optional can throw. Marking this unconditionally nonthrowing would turn an ordinary producer error into termination.

Add `std::exception_ptr error` and change the exception hook to:

```cpp
void unhandled_exception() noexcept { error = std::current_exception(); }
```

We now have somewhere to retain producer failure for delivery to the consumer.

## 3. Replace manual resume with `next()`

The consumer must distinguish a yield from completion. Add:

```cpp
bool next() {
    if (!handle_ || handle_.done()) return false;
    handle_.promise().current.reset();
    handle_.resume();
    if (handle_.promise().error)
        std::rethrow_exception(handle_.promise().error);
    return !handle_.done();
}
```

Why check twice? The first check avoids resuming a completed coroutine. The second detects whether that resume reached a yield or the end. Resetting the previous value makes the validity window explicit: advancing invalidates the current value before the producer runs again.

Add `value()` that returns `const T&`, but throws `logic_error` if the owner is empty, complete, or has no current value. The complete type is [generator.hpp](labs/07/generator.hpp). Compare each addition with your version before running it.

`value()` is a borrow of promise storage. Copy it if you need to keep it across the next `next()` call or generator destruction. The state is retained at final suspend, but that does not make the last yielded value part of our API after completion.

## 4. Replace printing with yielding

[02_range.cpp](labs/07/02_range.cpp) is now almost ordinary loop code:

```cpp
generator<int> numbers() {
    for (int n = 1; n <= 3; ++n) co_yield n;
}
```

The consumer uses:

```cpp
auto values = numbers();
while (values.next()) std::cout << values.value() << '\n';
```

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/07/02_range.cpp -o scratch/generator
./scratch/generator
```

Expected:

```text
1
2
3
done: 1
```

The last line comes from another `next()` after completion. It returns false without resuming. Falling off the producer body is valid because this promise has `return_void()`.

Change the loop bound to zero. Expect only `done: 1`; an empty generator is a valid sequence. Then call `value()` before the first successful `next()` inside a try/catch. Expect our diagnostic rather than an invalid optional dereference.

## 5. Make the producer fail after one value

[03_failure.cpp](labs/07/03_failure.cpp) yields `1`, then throws `runtime_error{"producer failed"}`:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/07/03_failure.cpp -o scratch/generator
./scratch/generator
```

Expected:

```text
1
producer failed
```

The second `next()` resumes the producer, which stores the exception and reaches final suspend. `next()` then rethrows it in the consumer. This is our first complete exception-transport path. A `co_await` on a task will later do the analogous check in `await_resume()`.

Do not retry this failed sequence expecting the producer to restart. It has completed exceptionally; a new call creates a new sequence.

## 6. Stop consuming before the producer finishes

In a local copy, add Lesson 3's destructor-logging guard before the loop. Call `next()` only once, read the value, and let the generator leave scope.

Expected: the guard's destructor runs during owner destruction; the loop's remaining iterations do not run. There are no external registrations, so abandoning this suspended producer is straightforward.

A conceptually infinite sequence also depends on this cleanup path. Choose a progression that does not introduce signed overflow; an unbounded signed Fibonacci loop eventually has undefined behavior even if coroutine ownership is correct. Our default example is finite.

## 7. Prevent an operation our pull interface cannot drive

The saved promise declares:

```cpp
template<class A> void await_transform(A&&) = delete;
```

Try adding `co_await std::suspend_always{};` before the first yield. Compilation should fail on the deleted transformation. Restore the file afterward.

Why prohibit it? `next()` assumes one resume either produces a value or finishes. An arbitrary timer/I/O pause could return with neither a value nor completion. An async generator needs an awaitable next operation, linked producer/consumer lifetimes, cancellation, and an executor contract. Our synchronous interface cannot silently become one.

`co_yield` still works because its implicit await bypasses ordinary `await_transform`, as you observed in Lesson 6. Initial and final suspension also continue to work.

## Extend the consumer interface without changing the engine

A `begin()` could call `next()` once; iterator increment could call it again; dereference could call `value()`; an end sentinel could reflect completion. A real iterator interface must meet the required iterator concepts. This sequence is single-pass: copying an iterator would still refer to the same producer, not create a second traversal. Exceptions would appear during `begin()` or increment.

Reference-yielding designs avoid some copies by borrowing the producer's value. They need a stronger lifetime contract for both lvalues and temporaries. We keep owned promise values so the capstone can pull file lines safely.

**Checkpoint:** explain the difference between the producer's local `n`, the promise's current value, and the consumer's copied value. Then explain who controls the rate of production. Here the consumer does: one request permits one value. That is the simplest form of backpressure.

```bash
python3 tools/check_labs.py 07
```

Next: [Lesson 8 — Arrange a future resume](08_creating_awaitables.md).
