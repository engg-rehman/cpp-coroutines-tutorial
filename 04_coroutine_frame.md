---
title: "Lesson 4: Repair a Lifetime Assumption"
description: "Use delayed execution to expose borrowed state, repair parameter and closure lifetimes, and inspect coroutine allocation."
keywords:
  - C++ coroutine frame
  - coroutine allocation elision
  - coroutine lifetime
  - dangling reference coroutine
  - coroutine memory
seo:
  type: TechArticle
---

# Lesson 4: Discover what survives suspension

Our coroutine keeps a local counter alive. Does it also preserve everything passed to the function? Let's try to print a snapshot of a string and find out.

## 1. Make a prediction that the program disproves

Open [01_borrow.cpp](labs/04/01_borrow.cpp):

```cpp
manual_job print_later(const std::string& text) {
    std::cout << text << '\n';
    co_return;
}
```

`main` creates `text = "original"`, calls `print_later(text)`, assigns `"changed"` to the string, and only then resumes the coroutine.

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/04/01_borrow.cpp -o scratch/lifetime
./scratch/lifetime
```

Expected output:

```text
changed
```

The frame retained a reference to the original string, not a snapshot of its contents. The referent is still alive, so this is a safe experiment with a surprising result. It exposes the ownership assumption without relying on a crash.

## 2. Fix the snapshot requirement with a value parameter

Change the parameter to `std::string text`. Build the saved repair:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/04/02_own.cpp -o scratch/lifetime
./scratch/lifetime
```

Expected:

```text
original
```

The caller initializes a value parameter, and the coroutine machinery creates the frame-resident parameter copy. A non-reference parameter copy is initialized from an xvalue referring to the parameter, which permits moving its resources. A reference parameter remains a reference to the same object. Copy elision and optimization can affect which constructor calls you observe; the ownership difference remains.

A borrowed reference is fine when the caller guarantees its lifetime and wants shared observation. It is wrong for this snapshot requirement.

## 3. Find the dangerous version before executing it

With the original reference parameter, consider this **unsafe edit, not a runnable checkpoint**:

```cpp
auto owner = print_later(std::string{"temporary"});
owner.resume(); // The string died at the preceding semicolon.
```

The returned owner preserves the frame, but the frame's reference does not preserve the temporary. Accessing it on resume is undefined behavior. It may print the expected text, garbage, or appear to work until optimization changes. The language does not promise a diagnostic.

Use the value-parameter version for this call. When deliberately investigating the unsafe variant in a separate local copy, AddressSanitizer can help find the misuse:

```bash
clang++ -std=c++20 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer scratch/lifetime.cpp -o scratch/lifetime
```

A sanitizer failure is evidence of a bug; a clean run does not prove a borrowed lifetime is valid. The regular checkpoint suite never executes undefined behavior.

## 4. Apply the same reasoning to a lambda

This is also **unsafe** if the coroutine later reads the capture:

```cpp
auto owner = [text = std::string{"hello"}]() -> manual_job {
    std::cout << text << '\n';
    co_return;
}();
owner.resume();
```

The temporary closure owns `text`. Calling the closure does not automatically move its captures into the coroutine frame. The coroutine may keep access through the closure's implicit object parameter, and the closure is gone at the first semicolon.

[03_closure.cpp](labs/04/03_closure.cpp) repairs it by using a captureless coroutine lambda with an explicit value parameter. It also shows a named free function accepting a value:

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/04/03_closure.cpp -o scratch/lifetime
./scratch/lifetime
```

Expected:

```text
parameter survives
named function survives
```

Another valid repair is to keep a named closure alive until its coroutine completes. Choose the design whose ownership is easiest to see.

A member coroutine has the same underlying risk: calling `session.run()` does not copy `session` into the frame. `this` must remain valid. A parent scope can own both objects; a shared-ownership design can explicitly retain the session, but must avoid reference cycles. Taking a `shared_ptr<session>` **by value** in a free coroutine makes that retention visible.

## 5. Inspect what the frame must retain

Add these locals to a copy of Lesson 2's coroutine:

```cpp
int used_before = 7;
std::cout << used_before << '\n';
int used_after = 9;
co_await std::suspend_always{};
std::cout << used_after << '\n';
```

`used_after` must remain available after suspension. `used_before` no longer needs storage for later execution. The compiler may fold either constant away. Do not conclude that every source local occupies a fixed frame slot.

In general, retained state includes the promise, parameter copies, necessary locals and temporaries, awaiters, and resume/destroy bookkeeping. No portable offsets or struct layout are specified. `sizeof(promise_type)` measures the promise object, not the whole frame.

Now replace a local with a destructor-logging guard and observe Lesson 3's rule: its lifetime follows its scope. A lock guard therefore stays locked if its scope crosses suspension. Reduce the locked scope before awaiting. Resuming later on another thread can additionally violate a mutex's ownership rules.

## 6. Ask the allocator what this compiler does

Copy `labs/02/manual_job.hpp` to `scratch/manual_job.hpp` and `labs/02/03_resume.cpp` to `scratch/allocation.cpp`. Add `<cstddef>`, `<cstdio>`, and `<new>` to the copied header. Add this inside its `promise_type`:

```cpp
static void* operator new(std::size_t bytes) {
    std::printf("allocate %zu bytes\n", bytes);
    return ::operator new(bytes);
}
static void operator delete(void* memory, std::size_t bytes) noexcept {
    std::printf("release %zu bytes\n", bytes);
    ::operator delete(memory);
}
```

Compile at two optimization levels:

```bash
clang++ -std=c++20 -O0 scratch/allocation.cpp -o scratch/allocation-debug
./scratch/allocation-debug
clang++ -std=c++20 -O2 scratch/allocation.cpp -o scratch/allocation-release
./scratch/allocation-release
```

There is deliberately no expected byte count. If allocation happens, the hooks reveal the requested storage. The compiler can omit coroutine allocation, including observable custom allocation calls, when it can establish the required nesting and size conditions. No allocation print is a possible outcome. An implementation can embed storage in the caller; a coroutine is not guaranteed to allocate on the heap.

Add a large nontrivial local used after suspension and compare. Record compiler, optimization flags, observed allocations, and bytes. Avoid making portable performance claims from one run.

For a specialized allocator, a promise allocation overload may accept the coroutine arguments after the size parameter. Those arguments must match the selected signature, and deallocation and construction-failure cleanup must agree. This is more than replacing one `new` expression in the body.

A promise can also declare `get_return_object_on_allocation_failure()`. Then the selected allocation path must be nonthrowing; a null allocation result returns the hook's sentinel object without executing the body. For our owner, a possible sentinel is `manual_job{handle_type{}}`, and its `resume()` returns false. Test this policy in an isolated copy using a matching `noexcept` allocation function; optimizers may elide allocation, so a forced-null experiment is not a portable guarantee of taking that path.

**Checkpoint:** explain separately where the state is stored, who destroys it, and which referenced objects it does not own. Those are three different questions.

```bash
python3 tools/check_labs.py 04
```

Next: [Lesson 5 — Build an awaiter by fixing compiler errors](05_suspension_resumption.md).
