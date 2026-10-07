---
title: "Lesson 11: Handle Failure, Cleanup, and Cancellation"
description: "Transport a child exception, fix an await inside catch, and cancel a scheduled job while letting cleanup finish."
keywords:
  - C++ coroutine exception handling
  - coroutine cancellation
  - std::stop_token coroutine
  - unhandled_exception
  - structured concurrency C++
seo:
  type: TechArticle
---

# Lesson 11: Let work fail and still clean up

Our happy path is working. Now fail a child operation, then request that a scheduled operation stop. These are separate protocols; neither is implemented by pretending the coroutine never existed.

## 1. Throw inside a child

[01_errors.cpp](labs/11/01_errors.cpp) has a child that throws before its `co_return`, and a parent with a catch around `co_await child()`.

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/11/01_errors.cpp -o scratch/errors
./scratch/errors
```

Expected:

```text
parent caught: read failed
result: -1
```

Follow the actual data path in [task.hpp](labs/09/task.hpp): the child's `unhandled_exception()` stores `current_exception()`, final suspension transfers to the parent, and the parent's awaiter rethrows from `await_resume()`. The parent's ordinary catch converts failure to `-1`.

The compiler does not automatically transport every coroutine failure back to its original caller. We implemented storage and rethrowing. With Lesson 2's terminating promise, the outcome would instead be termination.

Try removing the parent's catch in a local copy and catching around `work.run()` in `main`. Expect the same error message there. The parent stores the propagated exception in its own promise before its root consumer rethrows it.

If nobody observes a failed task's result, our educational type does not report that stored exception elsewhere. An application must decide whether every operation is joined, errors are logged, or an unobserved-error policy is enforced. Detached operations need an explicit error sink.

## 2. Try awaiting cleanup inside the handler

Build [02_await_in_catch.cpp](labs/11/02_await_in_catch.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/11/02_await_in_catch.cpp -o scratch/errors
```

Expected: a compiler error forbidding `co_await` in a handler. The desire to clean up is valid; the placement is not.

## 3. Save the failure and leave the handler

[03_async_cleanup.cpp](labs/11/03_async_cleanup.cpp) repairs the structure:

```cpp
std::exception_ptr error;
try {
    throw std::runtime_error("read failed");
} catch (...) {
    error = std::current_exception();
}
co_await cleanup();
if (error) std::rethrow_exception(error);
```

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/11/03_async_cleanup.cpp -o scratch/errors
./scratch/errors
```

Expected:

```text
cleanup awaited
read failed
```

The cleanup in this checkpoint completes immediately, but its placement also permits a genuinely suspending operation when a driver exists. If cleanup itself fails, this version propagates that newer failure. An application that must retain both errors needs an aggregation policy; losing the original error should be a deliberate decision.

Ordinary synchronous RAII cleanup still belongs in destructors. A destructor cannot itself become a coroutine, and frame destruction does not magically await asynchronous cleanup.

## 4. Request stop without destroying the owner

[cancel.hpp](labs/11/cancel.hpp) defines our cancellation outcome and check:

```cpp
struct cancelled : std::runtime_error {
    cancelled() : std::runtime_error("cancelled") {}
};
void check_stop(std::stop_token token) {
    if (token.stop_requested()) throw cancelled{};
}
```

[04_cancel.cpp](labs/11/04_cancel.cpp) takes a token **by value** and checks it before and after each scheduled turn:

```cpp
for (int n = 1; n <= 3; ++n) {
    check_stop(token);
    co_await loop.schedule();
    check_stop(token);
    std::cout << "item " << n << '\n';
}
```

Compile and run:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/11/04_cancel.cpp -o scratch/cancel
./scratch/cancel
```

Expected:

```text
item 1
stop requested
cleanup
cancelled
queue empty: 1
```

`main` starts the task, drives one turn, requests stop, and continues driving the loop. The coroutine resumes, observes stop, throws our cancellation exception, unwinds its local guard, and completes. `result()` makes that outcome visible to the caller. The owner is retained throughout.

A stop request is cooperative. It is not a forced frame deletion. There is no language `co_cancel`, no automatic parent-to-child token propagation, and no automatic interruption of blocking I/O. We pass the token explicitly.

## 5. Compare three shutdown paths

Make these edits separately and predict the output:

| Edit | Expected result |
|---|---|
| Request stop before `owner.start()` | No item prints; first check throws; guard still cleans up |
| Remove `stop.request_stop()` | Items 1, 2, 3 print; result is 3; cleanup occurs normally |
| Destroy the owner while queued, as in Lesson 10 | Guard cleans up and registration is removed, but no catch or result is delivered |

The last path is abandonment, not cooperative cancellation. Destruction does not execute the rest of the body, enter catches around future instructions, or call `unhandled_exception` as if cancellation had been thrown.

Our token checks work because the queue will deliver another turn. For an operation that can wait indefinitely, polling only after resumption cannot wake it. An adapter might register `std::stop_callback` to cancel/unregister the underlying wait and arrange a cancelled completion. That registration must itself remain valid and be safely removed or quiesced before its referenced state disappears.

## 6. Work through completion racing with stop

Take Lesson 8's operation-state sketch and add three terminal outcomes: success, error, cancelled. A completion callback and a cancellation callback can both arrive. Only one may change `pending` into a terminal outcome and schedule the continuation.

On paper, execute these two orders:

```text
pending -> success; later stop observes success and does nothing
pending -> cancelled; late success observes cancelled and does nothing
```

Now execute a third scenario: the callback has already taken the handle when destruction begins. Merely clearing a stored continuation is insufficient in a multithreaded implementation. Destruction must wait for or otherwise coordinate with that in-flight callback. Atomics/locks must also make the result visible before the continuation reads it.

The single-thread loop's RAII removal is a solution for its particular contract. It is not a universal callback cancellation algorithm.

A timeout adds a timer as another contender. The winner may report a result, but the loser can still have outstanding OS work. Cancel or detach the loser under a documented protocol and retain buffers/state until late completion is harmless. “The timeout returned” does not prove the read stopped touching its buffer.

## 7. Decide how multiple children fail together

In the final lesson we will start a bounded group and drain every child before reading their outcomes. That gives child lifetimes a clear boundary. More generally, structured concurrency requires children to finish or be cancelled and joined before the enclosing operation leaves its scope.

A `when_all`-style API must say whether the first error cancels siblings, whether cleanup is joined, which errors are retained, which executor delivers them, and how cancellation competes with failure. The keyword `co_await` cannot select those policies for the application.

**Checkpoint:** for each of our types, name the destination of an uncaught error. The early manual job terminates, the generator rethrows through `next()`, and the task rethrows through its result or await boundary. Early allocation/parameter/promise/initial-entry failures can still escape the original invocation, as Lesson 3 established.

```bash
python3 tools/check_labs.py 11
```

Next: [Lesson 12 — Build a bounded reading report](12_practical_patterns.md).
