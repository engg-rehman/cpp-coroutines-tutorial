---
title: "Lesson 6: Follow co_await Conversion and Policy"
description: "Fix a non-awaitable operand with operator co_await, then use await_transform and repair its missing fallback."
keywords:
  - how co_await works
  - C++20 co_await
  - await_transform
  - operator co_await
  - C++ awaiter selection
seo:
  type: TechArticle
---

# Lesson 6: Teach `co_await` to understand a request

Suppose an operation is described by a request object. It should not need to contain scheduler bookkeeping itself. We will separate that description from the awaiter that performs the protocol, then add a promise-specific policy.

## 1. Try awaiting a plain value

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/06/01_not_awaitable.cpp -o scratch/conversion
```

[01_not_awaitable.cpp](labs/06/01_not_awaitable.cpp) contains `int n = co_await 42;`. Expect a compile error: `int` has no awaiter protocol. The wording may mention `await_ready` or an invalid member access.

`co_await` does not mean “turn any value into a future.” We need to supply the adaptation.

## 2. Convert a request to the awaiter we already know

[awaitables.hpp](labs/06/awaitables.hpp) first defines an immediately ready `immediate` awaiter whose `await_resume()` returns an integer. This is Lesson 5's ready path with prints.

Add a request wrapper with this method:

```cpp
struct request {
    int value;
    immediate operator co_await() const noexcept {
        std::cout << "member conversion\n";
        return {value};
    }
};
```

The body's edit is `co_await request{42}`. Build [02_conversion.cpp](labs/06/02_conversion.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/06/02_conversion.cpp -o scratch/conversion
./scratch/conversion
```

Expected:

```text
member conversion
ready
result
42
```

The operand is an **awaitable** because the language can turn it into an awaiter. The returned `immediate` object actually supplies `await_ready`, `await_suspend`, and `await_resume`.

A free `operator co_await(request)` can serve the same purpose, including through argument-dependent lookup. The header also defines a distinct `free_request` type to demonstrate this. Member and non-member candidates participate in overload resolution; neither form has an unconditional priority. Ambiguous selection is a compile error. If no viable conversion exists, the awaitable itself must satisfy the awaiter protocol.

## 3. Let this coroutine type interpret integers

Return to `co_await 42`. Add this method to the promise:

```cpp
request await_transform(int n) const noexcept {
    std::cout << "transform\n";
    return {n};
}
```

The complete owner with that one policy is [restricted_job.hpp](labs/06/restricted_job.hpp). Build [03_transform.cpp](labs/06/03_transform.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/06/03_transform.cpp -o scratch/conversion
./scratch/conversion
```

Expected:

```text
transform
member conversion
ready
result
42
```

The print order is the point: promise transformation happens **before** awaiter conversion. The promise turns `42` into a `request`, the request supplies an `immediate`, and that awaiter performs the protocol.

This lets a coroutine type interpret domain values such as timer durations, inject an executor or stop token, or prohibit unsupported operations. Our integer policy is small enough to trace without a timer implementation.

## 4. Break an awaitable that used to work

Use the same promise but replace the body operand with `std::suspend_never{}`. Build [04_restricted.cpp](labs/06/04_restricted.cpp):

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/06/04_restricted.cpp -o scratch/conversion
```

Expected: failure to convert the argument to the `int` accepted by `await_transform`.

The promise now declares that name, so an ordinary source-level await must go through it. The compiler does not silently bypass the method because another operand already has `await_ready`. This is a real compatibility consequence of adding a policy hook.

## 5. Restore other awaitables deliberately

Add a forwarding overload beside the integer overload:

```cpp
template<class A>
A&& await_transform(A&& a) const noexcept {
    return std::forward<A>(a);
}
```

The repaired owner is [policy_job.hpp](labs/06/policy_job.hpp). [05_forward.cpp](labs/06/05_forward.cpp) awaits `free_request{7}` to exercise both the fallback and free conversion:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/06/05_forward.cpp -o scratch/conversion
./scratch/conversion
```

Expected:

```text
free conversion
ready
result
7
```

There is no `transform` print because that print belongs to the integer overload. In a local copy, try `std::suspend_never{}` again as a standalone expression, `co_await std::suspend_never{};`. It now compiles. Do not assign it to `int`: its result is `void`.

Forwarding preserves references and value categories; it does not create ownership. The temporary awaiter created for an await-expression survives its suspension until the expression completes. Objects merely referenced by it still need their own lifetime proof.

A generic tracing adapter needs more care than these print statements: preserve the inner awaiter's lifetime, all three `await_suspend` return forms, reference results, and required `noexcept` behavior. Storing a reference to a local conversion result would reintroduce Lesson 4's bug.

## 6. Notice what did not break

Our restricted promise could still initially suspend and finally suspend even though it accepts only integer operands. The compiler's initial and final awaits bypass ordinary `await_transform`. So does the implicit await used by `co_yield`.

In Lesson 7, we will exploit this by deleting ordinary `await_transform` in a synchronous generator. Yielding will still work, while arbitrary async awaits will be rejected.

The selection path you have now observed is:

```text
source operand
  -> promise.await_transform, if the promise declares it
  -> selected member/free operator co_await, if viable
  -> awaiter.await_ready
  -> awaiter.await_suspend, if not ready
  -> awaiter.await_resume, when continuing
```

There is no standard function called `get_awaiter`. Libraries sometimes implement one for the conversion step; it does not automatically perform the promise's transformation.

## 7. Try an invalid placement

In a local copy, put a standalone await directly inside a `catch` block. Expect a compile error. In Lesson 11 we will move asynchronous cleanup after the handler and retain the exception in `exception_ptr`.

In C++20, awaits belong in permitted suspension contexts within coroutine bodies. They cannot appear in default arguments, unevaluated contexts, static/thread-local block-variable initializers, or directly in handlers; in a declaration statement they belong in its initializer. An ordinary `try` body may await, and its handler may catch an await's error.

**Your experiment:** change `immediate::await_ready()` to false, keep its empty `void await_suspend`, and add a second owner resume to `main`. Expect `result` and the integer only after that second resume. Conversion selects the awaiter; it does not determine whether the selected awaiter suspends.

```bash
python3 tools/check_labs.py 06
```

Next: [Lesson 7 — Pull one value at a time](07_coyield_generators.md).
