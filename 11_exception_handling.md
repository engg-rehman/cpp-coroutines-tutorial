---
title: "C++ Coroutine Exceptions and Cancellation"
description: "Learn how C++ coroutine exceptions cross await boundaries and how cleanup, stop tokens, cancellation races, timeouts, and structured concurrency interact."
keywords:
  - C++ coroutine exception handling
  - coroutine cancellation
  - std::stop_token coroutine
  - unhandled_exception
  - structured concurrency C++
seo:
  type: TechArticle
---

# Lesson 11: Exceptions, Cleanup, and Cancellation

## Exception flow inside a coroutine

Ordinary `try`/`catch` works across suspension:

```cpp
task<response> load() {
    try {
        auto data = co_await read_data();
        co_return parse(data);
    } catch (const parse_error&) {
        co_return fallback_response();
    }
}
```

The handler remains part of the coroutine's generated control flow. Live RAII objects retain their lifetimes across suspension exactly as their scopes require.

One syntax restriction matters: `co_await` cannot appear directly in a handler. If asynchronous cleanup is needed, capture `std::exception_ptr`, leave the handler, await cleanup, and then rethrow or translate.

## Uncaught exceptions

If an exception escapes the user body, generated code calls:

```cpp
promise.unhandled_exception();
```

For a result-bearing task:

```cpp
void unhandled_exception() noexcept {
    exception = std::current_exception();
}
```

The task awaiter later does:

```cpp
if (promise.exception) {
    std::rethrow_exception(promise.exception);
}
```

Thus the error appears at the parent's `co_await`. See [`examples/exceptions.cpp`](examples/exceptions.cpp).

An exception thrown during very early coroutine creation or initial-suspend entry can instead propagate directly to the original caller. Promise construction and `get_return_object()` therefore also need deliberate exception specifications.

## Cleanup on completion and abandonment

- On normal or exceptional completion, live body locals are destroyed before final suspend.
- Destroying a suspended incomplete coroutine destroys its live locals without executing the rest of the body.
- Destruction does not invoke `unhandled_exception()` and does not run user `catch` blocks around code that never resumes.
- External operations are not automatically cancelled when the frame is destroyed.

An RAII registration stored in the frame can unregister a callback during destruction, but only if the external API guarantees that unregistration safely handles an in-flight callback.

## Cancellation is a protocol

C++ coroutine syntax has no `co_cancel` and no automatic parent-to-child cancellation. C++20's `std::stop_source`, `std::stop_token`, and `std::stop_callback` are useful building blocks, but the task and every awaited operation must agree to propagate and observe them.

Typical flow:

```text
owner requests stop
  -> child operation observes token or receives callback
  -> underlying wait is cancelled/unregistered
  -> operation completes exactly once with cancelled status
  -> coroutine resumes and handles/propagates cancellation
```

Cancellation may be represented by an exception, an error code, an empty result, or a distinct result variant. Consistency matters more than the particular representation.

## Cancellation races

Cancellation can race with successful completion. The operation state needs one atomic or locked winner:

```text
pending -> completed
pending -> cancelled
```

Only the winner stores the terminal result and schedules the continuation. The loser observes that the operation is already terminal and does not resume again.

The cancellation callback itself can race with destruction. Its registration must be removed or quiesced before the state it references disappears.

## Timeouts

A timeout is a race between the primary operation and a timer. Correct behavior requires:

- one terminal winner;
- cancellation of or detachment from the loser;
- no duplicate continuation resume;
- a policy for an underlying operation that cannot truly be cancelled;
- retention of state until any unavoidable late callback is harmless.

Returning from the timeout await does not prove the losing OS operation has stopped touching its buffers.

## Structured concurrency

Structured concurrency gives child operations lifetimes nested within a parent scope. Before the scope exits, children are completed or cancelled and joined. This model makes coroutine-frame ownership, exception aggregation, and shutdown substantially easier to reason about.

Detached work should be exceptional. It needs an independent owner, error sink, cancellation source, and application-shutdown policy.

## Exception aggregation

When operations run concurrently, more than one can fail. A `when_all`-style primitive must document:

- whether the first failure cancels siblings;
- whether it waits for siblings to finish cleanup;
- whether it returns one error or an aggregate;
- which executor delivers the result;
- what happens if cancellation also occurs.

Simple sequential `co_await` naturally reports one error at a time; concurrent composition introduces policy.

## Error-policy checklist

For every coroutine return type, answer:

1. Where is an uncaught exception stored or reported?
2. At which call or await does it reappear?
3. Can an exception be silently lost if the task is never awaited?
4. What does destroying an incomplete task do?
5. How are cancellation and ordinary failure distinguished?
6. How are errors from detached work observed?

Next: [Lesson 12 — Practical Patterns](12_practical_patterns.md)
