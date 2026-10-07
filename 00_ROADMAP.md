---
title: "C++20 Coroutines Workshop Roadmap"
description: "Twelve connected hands-on lessons, from a failing co_return to a bounded file-processing application, with a complete concept coverage map."
seo:
  type: TechArticle
---

# Your route through the workshop

Keep an editor beside the terminal. At each checkpoint, predict what will happen, run the command, explain the discrepancy, and make the next small edit. Read the explanation after seeing the problem it answers. The saved source is a recovery point; reconstructing it yourself is the exercise.

You should already know ordinary C++ functions, object lifetime, RAII, references, move semantics, templates, and exceptions. Concurrency concepts are introduced through observable one-thread experiments before we discuss cross-thread constraints.

## The progression

| Lesson | Problem that motivates the change | What you build or observe |
|---|---|---|
| [1. Make `co_return` compile](01_what_are_coroutines.md) | A rejected `void` coroutine | A complete promise built one diagnostic at a time |
| [2. Pause, resume, and retain local state](02_why_coroutines.md) | A counter restarts; a lazy job never starts | Two cooperatively interleaved counters |
| [3. Trace creation and cleanup](03_coroutine_lifecycle.md) | Completion is mistaken for destruction | Instrumented completion and abandonment |
| [4. Repair lifetime assumptions](04_coroutine_frame.md) | A delayed print sees changed data | Owned parameters, safe closures, and allocation experiments |
| [5. Build an awaiter](05_suspension_resumption.md) | An empty pause type does not compile | Ready, suspended, and cancelled-suspension paths |
| [6. Adapt an awaitable](06_coawait_mechanics.md) | An integer cannot be awaited; a policy rejects old code | Traced conversion and a repaired forwarding policy |
| [7. Pull values from a generator](07_coyield_generators.md) | The promise cannot yield | A finite pull sequence with value and error delivery |
| [8. Arrange the next resume](08_creating_awaitables.md) | A queued job cannot finish without a driver | A ready queue and a single-thread callback adapter |
| [9. Compose result-bearing tasks](09_creating_coroutine_types.md) | A result cannot be awaited; a child loses its parent | Lazy `task<T>` and `task<void>` with continuations |
| [10. Repair queued-handle ownership](10_coroutine_handle.md) | Destroying an owner can leave a stale registration | RAII queue removal and safe abandonment |
| [11. Handle errors and cancellation](11_exception_handling.md) | Awaiting in a catch fails; stop alone does not finish work | Exception transport, async cleanup placement, and cooperative stop |
| [12. Build a bounded reading report](12_practical_patterns.md) | Invalid input, sequential scheduling, and shutdown | A file-driven application combining the preceding pieces |

By Lesson 2, a suspended function can keep its local state. By Lesson 7, a consumer can pull values. By Lesson 9, a parent can await a child's result. By Lesson 12, those pieces cooperate in an application with a bounded amount of outstanding work, error reporting, and cancellation.

## Keep the details attached to an experiment

The practical sequence retains the original tutorial's concept coverage. Some advanced extensions, such as a fully concurrent I/O adapter or timeout engine, are analyzed as explicit design exercises; the workshop does not pretend its small runtime implements them.

| Concept group | Where you encounter and investigate it |
|---|---|
| Coroutine classification; `co_return`; promise vs return object; traits selection; member parameter types; language restrictions | Lesson 1 compiler ladder and edits |
| Why preserve control flow; state machines, callbacks, threads, futures; suspension, concurrency, parallelism, asynchrony; library responsibilities and tradeoffs | Lesson 2 counter and interleaving experiments |
| Creation order; eager/lazy start; normal/exceptional completion; initial/final awaits; abandonment; creation failures | Lesson 3 instrumented trace and initial-suspend edit |
| Frame contents/layout; live locals; parameter copies; references; `this`; closures; RAII; allocator hooks, elision, and failure | Lesson 4 snapshot failure, lifetime repairs, allocation probes |
| All awaiter methods; `void`/`bool`/handle suspension results; value/reference results; protocol exceptions; publication and cross-thread constraints | Lesson 5 compiler ladder, path trace, and throw experiments; transfer implemented in Lesson 9 |
| `await_transform` before member/free conversion; fallback and ambiguity; temporary lifetimes; implicit awaits; placement restrictions | Lesson 6 conversion trace and rejected policy fallback; Lesson 11 catch repair |
| `co_yield`; `yield_value`; finite/empty/infinite sequences; current-value lifetime; exceptions; early cleanup; single-pass iterators; async-generator boundary | Lesson 7 generator build and mutations |
| Ready operations; scheduler queues; inline/deferred completion; duplicate and late callbacks; registration races; operation-state lifetime; errors | Lesson 8 loop and callback runs; threaded protocol requirements are analyzed, not implemented as a production adapter |
| Result storage; lazy tasks; `return_value`; void specialization; continuation; symmetric transfer; child temporaries; single-consumer enforcement; limited sync driving | Lesson 9 missing-result and lost-parent checkpoints |
| Typed/erased/null/noop handles; construction and opaque addresses; preconditions; moves; unique/self/shared ownership; queue unregistration; continuation ownership | Lesson 10 copy failure, address round trip, abandonment repair |
| Exception transport; cleanup; unobserved failures; stop tokens/callbacks; cancellation races; timeout losers; structured concurrency and error aggregation | Lesson 11 runnable errors and stop, followed by explicit race/shutdown exercises |
| Sequential/concurrent composition; bounded backpressure; child joining; executor affinity; blocking resources; immediate completion; debugging; performance and production contracts | Lesson 12 file-processing application and modifications; allocation measurement begins in Lesson 4 |

## Checkpoints and recovery

Each lesson links complete saved sources in `labs/NN/`. A build failure is intentional only when the lesson says so. The machine-readable [checkpoint manifest](labs/checkpoints.json) records expected diagnostic concepts or output. [`tools/check_labs.py`](tools/check_labs.py) repeats those checks with timeouts so a missing resume cannot hang verification.

```bash
python3 tools/check_labs.py 09
python3 tools/check_labs.py --sanitize
```

To repeat a lesson, create a fresh file in `scratch/` and rebuild its steps without copying the finished type first. Explain who owns every retained object and who will resume each suspension. Compare your explanation against the next run, not only the final source.

## Language references for deeper checks

These C++20 draft references support the language boundaries used in the experiments:

- [Coroutine definitions, promise selection, parameter copies, and lifecycle](https://timsong-cpp.github.io/cppwp/n4861/dcl.fct.def.coroutine)
- [Await-expression transformation and suspension behavior](https://timsong-cpp.github.io/cppwp/n4861/expr.await)
- [Coroutine handle operations](https://timsong-cpp.github.io/cppwp/n4861/coroutine.handle)
- [Coroutine return statements](https://timsong-cpp.github.io/cppwp/n4861/stmt.return.coroutine)

The compiler demonstrates one implementation's diagnostics and behavior. The language contract explains which observations are portable; frame sizes, allocation elision, and exact messages are not fixed tutorial outputs.
