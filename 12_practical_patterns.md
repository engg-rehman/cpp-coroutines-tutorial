---
title: "Practical C++ Coroutine Design Patterns"
description: "Production-focused C++ coroutine guidance for executors, concurrency, backpressure, ownership, reentrancy, debugging, performance, and shutdown."
keywords:
  - C++ coroutine design patterns
  - coroutine executor
  - structured concurrency
  - coroutine performance
  - async C++ best practices
seo:
  type: TechArticle
---

# Lesson 12: Practical Coroutine Design

## Separate mechanism from policy

A maintainable coroutine system normally has distinct layers:

```text
coroutine return types (`task<T>`, generator<T>)
        |
awaitables/adapters (timer, socket read, channel receive)
        |
executor/reactor (ready queue, I/O polling, thread pool)
        |
OS or application event sources
```

The language transformation connects these layers but does not replace any of them.

## Make executor transitions visible

An awaitable such as `schedule_on(executor)` should explicitly communicate a possible thread/context switch:

```cpp
co_await schedule_on(cpu_pool);
auto parsed = parse(input);
co_await schedule_on(ui_executor);
display(parsed);
```

Document whether ordinary operation awaiters preserve the current executor, resume on an I/O thread, or use a supplied executor. Hidden migration makes thread-local state, locks, and UI affinity difficult to reason about.

## Avoid locks across suspension

Suspension may last indefinitely and resumption may occur on another thread. Do not hold a mutex, borrowed buffer lease, database transaction guard, or other scarce synchronous resource across `co_await` unless the abstraction explicitly supports it.

Use scopes to release first:

```cpp
request copy;
{
    std::lock_guard lock(mutex);
    copy = pending_request;
}
co_await send(copy);
```

## Expect synchronous completion and reentrancy

Any awaited operation may already be ready. A callback API may also invoke its callback inline during registration. Code must work whether completion is immediate or delayed.

Consequences include:

- do not assume code after starting a task runs before the task body;
- do not hold a lock while invoking code that may resume a coroutine needing the same lock;
- set state invariants before publishing a continuation;
- test immediate-completion paths deliberately.

## Sequential versus concurrent composition

Sequential:

```cpp
auto first = co_await read_one();
auto second = co_await read_two();
```

Concurrent intent:

```cpp
auto first = read_one();
auto second = read_two();
auto both = co_await when_all(std::move(first), std::move(second));
```

Whether the second form actually starts both operations depends on eager/lazy task policy and `when_all`. Prefer an explicit library primitive whose start, failure, and cancellation semantics are documented.

## Backpressure

An async producer must not outrun its consumer without bound. Common strategies are:

- bounded channels;
- pull-based async generators;
- semaphore/credit protocols;
- batching with explicit limits.

Every suspension point should correspond to a resource policy. An unbounded queue merely moves the waiting cost from threads to memory.

## Lifetime patterns that scale

Prefer:

- move-only tasks for one consumer;
- parent scopes that own and join children;
- operation states that outlive unavoidable callbacks;
- values in coroutine parameters when ownership is intended;
- explicit executor and cancellation context.

Be suspicious of:

- detached tasks with no error sink;
- raw handles stored in long-lived global queues;
- capturing coroutine lambdas invoked as temporaries;
- member coroutines outliving their object;
- callbacks that resume without a one-shot state transition;
- destroying a task as an undocumented cancellation mechanism.

## Performance workflow

Optimize only after measuring:

1. outstanding coroutine count and state-allocation rate;
2. state size distribution for hot coroutine functions;
3. ready-queue contention and wakeups;
4. synchronous-completion percentage;
5. context/thread migrations;
6. cache misses and retained memory;
7. end-to-end latency under cancellation and overload.

Useful optimizations can include:

- reducing values live across suspension;
- pooling operation states when profiling justifies it;
- symmetric transfer for task chains;
- batching scheduler queue operations;
- preserving executor affinity;
- using ready fast paths without breaking registration-race correctness.

Allocation elision is a compiler opportunity, not an API guarantee.

## Debugging workflow

When a coroutine hangs, draw the continuation graph:

```text
which coroutine is suspended?
  -> on which awaiter?
  -> who owns the state?
  -> who holds the continuation?
  -> what event should schedule it?
  -> can cancellation or shutdown remove that event?
```

Log operation IDs, suspension reason, executor, source thread, destination thread, and terminal transition. A stack trace alone is often insufficient because suspended coroutines are not represented as active native stacks.

Use sanitizers. AddressSanitizer finds many stale-frame and dangling-reference bugs. ThreadSanitizer can reveal result-publication and double-resume races, though custom synchronization may need annotations.

## Production readiness checklist

### Ownership

- Exactly one entity destroys each frame, or self-destruction is explicit.
- Queues/callbacks cannot retain a resumable handle past frame lifetime.
- member-object and lambda-closure lifetimes are proved.

### Completion

- Immediate and delayed completion behave equivalently.
- Exactly one terminal transition resumes the continuation.
- A coroutine at final suspend is never resumed.

### Errors and cancellation

- Uncaught exceptions have an observable destination.
- Cancellation reaches underlying operations where possible.
- Shutdown cancels/joins children and quiesces callbacks.
- Timeout losers cannot later access freed memory.

### Execution context

- Executor/thread-affinity rules are documented.
- No incompatible lock or thread-bound resource crosses suspension.
- Result publication synchronizes with resumption.

### API semantics

- Eager/lazy start is clear.
- Single- versus multi-consumer behavior is enforced.
- Detachment, abandonment, and destruction are defined.
- Async composition primitives specify sibling failure behavior.

## Where to go next

Study a mature library's task, scheduler, cancellation, and I/O adapter together; looking only at its `promise_type` misses most of the design. Compare its documented contracts against the checklist above, then trace one operation from creation through shutdown.

Return to the [roadmap](00_ROADMAP.md) or review the [correctness audit](CORRECTNESS_AUDIT.md).
