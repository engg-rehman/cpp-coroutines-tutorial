---
title: "Lesson 1: Make co_return Compile"
description: "Build a C++20 coroutine from a failing co_return, adding each promise method in response to compiler diagnostics."
keywords:
  - what are C++ coroutines
  - C++20 coroutines
  - promise_type
  - co_await
  - co_yield
  - coroutine_handle
seo:
  type: TechArticle
---

# Lesson 1: Make `co_return` compile

Open an editor and a terminal in this repository. We will write a function that prints `hello`. The first version will not compile. Every change below answers a problem the compiler exposes. You do not need to understand a finished coroutine type before starting.

Use a C++20 compiler and standard library with `<coroutine>` support. The commands use `clang++`; you can substitute a suitable `g++`. Create a place for your experiments:

```bash
mkdir -p scratch
```

All commands in the tutorial run from the repository root. `scratch/` is your editable workspace. Saved checkpoints under `labs/` let you recover without losing the sequence. Diagnostics below show the **important part of the message**, not a promise about exact wording or diagnostic order. Compilers may report several missing methods together.

## 1. Try the smallest function

Create `scratch/hello.cpp`:

```cpp
#include <coroutine>

void hello() {
    co_return;
}

int main() {
    hello();
}
```

Compile it:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic scratch/hello.cpp -o scratch/hello
```

**Expected: compilation fails**, mentioning `std::coroutine_traits<void>` and a missing `promise_type`. Compare [checkpoint 01](labs/01/01_void.cpp).

An ordinary `return;` finishes a `void` function using the ordinary function rules. Writing `co_return;` asks the compiler to transform the function into a coroutine. The compiler now needs a type that tells it what to do at creation, completion, and suspension. `void` supplies no such type.

Adding `<coroutine>` supplies the library declarations. It does not invent a coroutine return type for us.

## 2. Give the compiler a place to find the policy

Replace `void hello()` with `job hello()` and put this before the function:

```cpp
struct job {
    struct promise_type {};
};
```

Compile again. **It still fails**, now about missing members of `job::promise_type`. You have fixed the first problem. [Checkpoint 02](labs/01/02_empty_promise.cpp) records this version.

Why that exact nested name? For `R function(Args...)`, the compiler consults `std::coroutine_traits<R, Args...>::promise_type`. Its usual behavior is to find `R::promise_type`. We chose `job` as `R`, so `job::promise_type` is the customization object it will construct for each invocation. This is unrelated to `std::promise`.

You can specialize `std::coroutine_traits` for appropriate user-defined types; we use the nested name because it keeps the policy beside the return type. A non-static member coroutine also contributes its implicit object-parameter type to the traits arguments. You will see why member functions need special lifetime care in Lesson 4.

## 3. Decide what the caller receives

Add this inside the empty promise:

```cpp
job get_return_object() noexcept { return {}; }
```

Compile again. Missing suspension/completion methods still prevent compilation. [Checkpoint 03](labs/01/03_return_object.cpp) contains the edit.

`hello()` is declared to return a `job`. `get_return_object()` creates that object for the caller. The promise object and the returned `job` are **different objects**. Our returned object is empty for now; later it will carry a way to resume the coroutine.

## 4. Decide whether the body starts immediately

Add:

```cpp
std::suspend_never initial_suspend() const noexcept { return {}; }
```

Compile again. It still fails. [Checkpoint 04](labs/01/04_initial_suspend.cpp) has the current version.

Before executing our body, the compiler asks the promise for an initial suspension operation. `suspend_never` says to keep going. We want `hello()` to execute during the call, so that is the simplest choice. Lesson 2 changes it and observes the result.

## 5. Decide what happens at completion

Add:

```cpp
std::suspend_never final_suspend() const noexcept { return {}; }
```

Compile again. Completion and exception hooks are still missing. See [checkpoint 05](labs/01/05_final_suspend.cpp).

For this first example, completion should also keep going through cleanup: no suspended state remains. This works because our empty `job` holds no handle that could become stale. **When we introduce an owning handle in Lesson 2, we will change final suspension to retain the state.** These choices belong together.

The final suspension operation must not throw. That is why this method, and the final awaiter protocol, use `noexcept`.

## 6. Give `co_return;` its meaning

Add:

```cpp
void return_void() const noexcept {}
```

Compile again. The exception hook is still missing. See [checkpoint 06](labs/01/06_return_void.cpp).

`co_return;` asks the promise to run `return_void()`. Our function has no result to store, so the method is empty. Later, `co_return 42;` will require a different hook, `return_value(42)`.

The empty body is not pointless boilerplate: it states this coroutine's completion policy. It is also why falling off this coroutine's body will be allowed. Falling off a value-returning coroutine without a valid `return_void` path is undefined behavior, not an implicit default result.

## 7. Choose an uncaught-exception policy

Add `#include <exception>` and this promise method:

```cpp
void unhandled_exception() const noexcept { std::terminate(); }
```

The compiler needs a policy even though our tiny body does not explicitly throw. For this first type, an exception escaping the body terminates the program. We will replace this with stored exceptions for generators and tasks.

Now add `#include <iostream>` and print inside `hello()`:

```cpp
job hello() {
    std::cout << "hello\n";
    co_return;
}
```

Compile and run:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic scratch/hello.cpp -o scratch/hello
./scratch/hello
```

Expected output:

```text
hello
```

Your complete program should match [checkpoint 07](labs/01/07_hello.cpp). Read it now: every method exists because you encountered a specific requirement.

## 8. Make two small changes yourself

First replace `co_return;` with `return;`. Compile. Ordinary `return` is forbidden in a function that is a coroutine—but with no remaining `co_` keyword, this particular function becomes an ordinary `job` function, so `return;` instead fails because it supplies no `job`. Keep another `co_` keyword in the body and the diagnostic changes to the coroutine restriction. Classification depends on the whole function body.

Restore the program. Replace `co_return;` with `co_return 42;`. Compile. You should see a missing `return_value` diagnostic. Do not add that hook alongside `return_void`; declaring both is rejected. We will build a value-returning type in Lesson 9.

Restore `co_return;` before continuing.

## What you have actually built

You have a coroutine that runs immediately, prints, completes, and cleans up without leaving suspended work behind. It has not created a thread or scheduled anything.

The language identifies a coroutine from `co_return`, `co_await`, or `co_yield` in its body. We used the first. We will discover the other two by needing to pause and to produce successive values.

The restrictions also become easier to place now: `main`, constructors, destructors, C++20 `constexpr`/`consteval` functions, functions with placeholder return types, and C-style variadic functions cannot be coroutines. Variadic templates can be. Keep `main` ordinary and have it call your coroutine, as we did here.

**Before moving on:** close the finished source and reconstruct the five methods from the failures. Explain separately what creates the object received by `main`, what handles `co_return;`, and what handles an uncaught exception.

To check every saved step, including expected failures:

```bash
python3 tools/check_labs.py 01
```

Next: [Lesson 2 — Make a function remember where it stopped](02_why_coroutines.md).
