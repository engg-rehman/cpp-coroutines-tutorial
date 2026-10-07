---
title: "Lesson 5: Build an Awaiter One Error at a Time"
description: "Implement the three awaiter methods and trace ready, suspended, and cancelled-suspension paths."
keywords:
  - C++ awaiter protocol
  - await_ready
  - await_suspend
  - await_resume
  - coroutine resumption
  - symmetric transfer
seo:
  type: TechArticle
---

# Lesson 5: Replace the magic pause with your own awaiter

We have used `std::suspend_always` without implementing it. Now replace it with a type named `pause` and let the compiler tell us what is missing.

## 1. An empty type does not know how to pause

[01_missing_ready.cpp](labs/05/01_missing_ready.cpp) contains:

```cpp
struct pause {};
manual_job work() {
    co_await pause{};
    std::cout << "after\n";
}
```

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/05/01_missing_ready.cpp -o scratch/awaiter
```

Expected: compilation fails, mentioning `await_ready`. The coroutine return type already works; this error belongs to the operand of `co_await`.

The object the compiler uses to perform an await is called the **awaiter**. Here `pause{}` is directly that object. Lesson 6 will add a conversion between the operand and the awaiter.

## 2. Answer the readiness question

Add:

```cpp
bool await_ready() const noexcept { return false; }
```

Compile [02_missing_suspend.cpp](labs/05/02_missing_suspend.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/05/02_missing_suspend.cpp -o scratch/awaiter
```

Expected: missing `await_suspend`.

`false` means this operation is not ready to complete immediately. The result need only be contextually convertible to `bool`. The compiler still needs to know what to do after it suspends the coroutine.

## 3. Accept the suspended coroutine's handle

Add:

```cpp
void await_suspend(std::coroutine_handle<>) const noexcept {}
```

Compile [03_missing_resume.cpp](labs/05/03_missing_resume.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/05/03_missing_resume.cpp -o scratch/awaiter
```

Expected: missing `await_resume`.

The argument is a handle to the awaiting coroutine. We accept an erased handle because this operation does not need the promise type. A more specialized awaiter may accept a typed handle.

The coroutine is already considered suspended when `await_suspend` is called. Our empty method leaves it suspended and does not register it anywhere. This is sufficient only because `main` already owns the coroutine and will manually resume it.

A scheduler awaiter will store that handle; a task awaiter will return another handle. We will build both.

## 4. Finish the await-expression

Add:

```cpp
void await_resume() const noexcept {}
```

Now build and run [04_pause.cpp](labs/05/04_pause.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/05/04_pause.cpp -o scratch/awaiter
./scratch/awaiter
```

Expected output: `after`, once. The first owner resume starts the body and reaches `pause`. The second completes that await-expression and executes the print.

`await_resume()` is the last part of evaluating `co_await`, before execution proceeds past the expression. Its return value becomes the result of the whole await. Our pause has no result, so it returns `void`.

## 5. Return a value and trace three different paths

[05_paths.cpp](labs/05/05_paths.cpp) replaces `pause` with:

```cpp
struct value_awaiter {
    bool ready;
    bool stay_suspended;
    bool await_ready() const noexcept {
        std::cout << "ready\n";
        return ready;
    }
    bool await_suspend(std::coroutine_handle<>) const noexcept {
        std::cout << "suspend\n";
        return stay_suspended;
    }
    int await_resume() const noexcept {
        std::cout << "resume\n";
        return 42;
    }
};
```

The body assigns `int result = co_await value_awaiter{ready, stay};`. Before running, predict which methods print for `(true, false)`, `(false, false)`, and `(false, true)`.

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/05/05_paths.cpp -o scratch/awaiter
./scratch/awaiter
```

Expected output, grouped here into the three experiments:

```text
start
ready
resume
value: 42
back in main
start
ready
suspend
resume
value: 42
back in main
start
ready
suspend
back in main
resume
value: 42
```

The first path never suspends. The second reaches `await_suspend` but returns `false`, cancelling suspension so evaluation continues directly with `await_resume`. It does not mean the compiler makes a separate `handle.resume()` call. The third stays suspended until the caller resumes it again.

This also disproves “every `co_await` gives control back to the caller.” It may finish synchronously.

## 6. Choose the appropriate suspend result

You have implemented `void` and `bool` forms. The third legal form returns a coroutine handle:

```cpp
std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
    child.promise().continuation = parent;
    return child;
}
```

This is a preview fragment; `child` will be introduced in Lesson 9. Returning the child resumes it while the parent remains suspended. Returning from the child's final await to the parent completes the chain. This is called symmetric transfer. It expresses a control transfer; a particular machine-code tail call or stack-depth guarantee is not portable.

`std::noop_coroutine()` is a valid handle to return when no continuation needs work. Returning a null handle is not the equivalent of a noop continuation.

| Return from `await_suspend` | What happens after a normal return |
|---|---|
| `void` | Current coroutine remains suspended; caller/resumer regains control |
| `true` | Current coroutine remains suspended |
| `false` | Suspension is cancelled; continue with `await_resume` |
| A coroutine handle | Resume the returned coroutine; current coroutine stays suspended |

## 7. Make the operation fail deliberately

In a local copy of `05_paths.cpp`, put a `try`/`catch(const std::runtime_error&)` around the `co_await` and include `<stdexcept>`. Remove `noexcept` from the method you change, then throw `std::runtime_error{"probe"}` from one method at a time.

Expected observations:

- Throw from `await_ready`: the body's catch runs without suspension; `await_suspend` and `await_resume` do not print.
- Throw from `await_suspend`: suspension is undone and the exception appears at the await-expression; `await_resume` does not run for that failed await.
- Throw from `await_resume`: it is an ordinary expression failure at the await boundary.

If you omit the body catch, our current `manual_job` reaches `unhandled_exception` and terminates. If you retain `noexcept` while throwing, termination follows the noexcept rule before that recovery path. These are different experiments.

`await_resume` can also return a reference. The await-expression preserves the corresponding result category; returning a reference does not extend the referenced object's lifetime.

## A constraint to carry into the next lessons

Once `await_suspend` publishes a handle to code that can resume it, the coroutine may run and destroy the awaiter before `await_suspend` returns. On another thread this can happen concurrently. Arrange invariants before publication and do not touch frame-backed members afterward unless the protocol proves they remain valid. If registration throws after publishing a live handle, you also need to revoke that registration; otherwise a later callback can resume an already-continued coroutine.

Cross-thread designs must publish results with synchronization and preserve the frame's lifetime. Thread-local state, UI affinity, and locks may change meaning after migration. C++20 additionally has execution-agent restrictions: resumption across agents is implementation-defined outside the standard thread/main cases. Our examples stay on one thread; the later callback lesson will make its limits explicit.

**Checkpoint:** explain why printing `resume` does not prove the coroutine actually suspended, and why `done()` cannot make a dangling or concurrently running handle safe.

```bash
python3 tools/check_labs.py 05
```

Next: [Lesson 6 — Find out which object `co_await` really uses](06_coawait_mechanics.md).
