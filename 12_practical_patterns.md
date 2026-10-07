---
title: "Lesson 12: Build a Bounded Data Processing Application"
description: "Combine the generator, task, event loop, and stop token into a file-reading report with bounded concurrency and observable errors."
keywords:
  - C++ coroutine design patterns
  - coroutine executor
  - structured concurrency
  - coroutine performance
  - async C++ best practices
seo:
  type: TechArticle
---

# Lesson 12: Build a bounded reading report

We now have enough machinery to solve a small complete problem: read a file of measurements, validate each measurement, and report the total without starting unlimited work. You will compare sequential and concurrent scheduling, inject invalid input, and cancel a batch.

The file reader uses ordinary synchronous file I/O. A scheduling turn simulates a service that completes later. This makes every ordering reproducible without credentials, network timing, or a coroutine library. It does not make `ifstream` asynchronous, and the queue runs on one thread.

## 1. Start with the application requirement

Each nonempty input line should be an integer from 0 to 100. Invalid lines are reported and skipped. A report counts accepted, rejected, and cancelled work and totals valid values. A width parameter caps outstanding records at 1 through 8.

The fixture [readings.txt](labs/12/readings.txt) contains:

```text
10
20
bad
30
40
```

We expect four accepted readings, one rejected reading, and total 100. The third line is deliberate bad input; it should not discard the later valid readings.

Create your own `scratch/report.cpp`, then build the following pieces in order. The complete recovery checkpoint is [01_report.cpp](labs/12/01_report.cpp). If working from `scratch/`, include the headers via `../labs/07/generator.hpp`, `../labs/09/task.hpp`, `../labs/10/event_loop.hpp`, and `../labs/11/cancel.hpp`; the saved file uses paths relative to `labs/12/`.

## 2. Pull lines without reading the whole file first

Use Lesson 7's generator:

```cpp
generator<std::string> lines(std::string path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open input");
    std::string line;
    while (std::getline(input, line)) co_yield line;
    if (input.bad()) throw std::runtime_error("input read failed");
}
```

First write a temporary `main` that loops over `next()` and prints `value()`. Compile and confirm that all five fixture lines appear.

The path is owned by value. The stream and current line stay alive in the producer across yields. Leaving the generator's scope closes the file even if the consumer stops early. Reading a nonexistent file throws when the lazy generator is first advanced, not when `lines(path)` creates its owner.

## 3. Process one owned line

Add a `task<int> parse_reading(event_loop&, stop_token, string text, int id)`. Check stop, print `start ID`, await `loop.schedule()`, check stop again, parse the string, and print `finish ID` before returning a valid integer.

The saved implementation uses `std::from_chars`, verifies complete consumption of the input, and rejects values outside 0–100. `text` is a value parameter: advancing the generator must not invalidate a suspended record's input. Passing its `value()` reference directly through a borrowing parameter would recreate Lesson 4's problem.

Before adding batches, drive one task explicitly:

```cpp
auto job = parse_reading(loop, stop.get_token(), "10", 1);
job.start();
loop.run();
std::cout << job.result() << '\n';
```

Expected: `start 1`, `finish 1`, then `10`. Change the text to `bad` and catch the result error. Expect `start 1` and `invalid reading`, with no finish print. The parse failure crosses the result boundary exactly as Lesson 11 taught.

The saved application also wraps this child in `process()`, which uses `co_await parse_reading(...)`. That exercises parent/child continuations while the child is scheduled externally: a queue turn resumes the child, its final await resumes the parent, and the root becomes complete.

## 4. Establish the sequential baseline

Build the complete checkpoint and choose width 1:

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/12/01_report.cpp -o scratch/report
./scratch/report labs/12/readings.txt 1
```

Expected:

```text
start 1
finish 1
start 2
finish 2
start 3
reject: invalid reading
start 4
finish 4
start 5
finish 5
accepted=4 rejected=1 cancelled=0 total=100
```

At most one record is in progress. Suspending inside that record does not automatically start the next one. This is the practical reason two sequential `co_await` expressions are still sequential.

## 5. Start a bounded group before driving the loop

Run the same binary with width 2:

```bash
./scratch/report labs/12/readings.txt 2
```

Expected:

```text
start 1
start 2
finish 1
finish 2
start 3
start 4
finish 4
reject: invalid reading
start 5
finish 5
accepted=4 rejected=1 cancelled=0 total=100
```

Two starts before either finish demonstrate concurrent work in progress. The loop still executes one coroutine at a time; there is no parallelism or claimed CPU speedup.

Inspect the batch loop:

```cpp
std::vector<task<int>> batch;
for (int i = 0; i < width && source.next(); ++i)
    batch.push_back(process(loop, token, source.value(), ++id));
for (auto& job : batch) job.start();
loop.run();
for (auto& job : batch) {
    // Read each result, recording either success or failure.
}
```

This fragment uses `token` as shorthand for the saved program's `stop.get_token()`.

Creating lazy tasks alone starts nothing. The explicit start loop creates overlap. We finish building the vector before starting any work, and it owns every root until the queue drains. Each root owns its child temporary. The event loop outlives both.

Every started child is completed before results are inspected and errors handled. A rejected reading does not skip sibling cleanup. Our error policy records each failure and continues. A different application could cancel siblings on the first failure, but would still need to join them.

## 6. Explain the memory bound, then find its limit

The consumer advances the generator only enough to fill one batch. Therefore at most `width` root records are outstanding, with one child task per root. The producer's retained line and each owned record string also consume memory.

This is backpressure through bounded pulling. It bounds record count, not arbitrary input size: a single enormous line can still allocate a large string. A hostile-input service would need a line-size limit as well.

Try width 3 and predict the grouping before running. The accepted count and total should be unchanged, while starts and finishes interleave differently. A barrier after each batch can underutilize a real service if one record is slow. A sliding window, semaphore, or bounded channel would admit another item when any slot becomes free. Those are extensions with additional scheduling and shutdown contracts, not free consequences of `co_await`.

## 7. Cancel after starting a batch

```bash
./scratch/report labs/12/readings.txt 2 cancel
```

Expected:

```text
start 1
start 2
accepted=0 rejected=0 cancelled=2 total=0
```

The application requests stop before pumping the queued continuations. Each child resumes, checks the shared token, and completes with cancellation. The loop drains, results are collected, and no further input batch is pulled. Owners are still alive until their work has stopped.

This checks cancellation, error transport, parent continuation, and structured cleanup together. Lesson 10's registration removal also provides safe unwinding if an owner is abandoned while queued under this loop's single-thread contract.

## 8. Exercise the boundary cases yourself

| Input or command change | Expected observation |
|---|---|
| Empty input file | All counts and total are zero |
| A line containing `12x`, `-1`, or `101` | That record is rejected |
| Blank line | Rejected as an invalid reading |
| Nonexistent input path | `cannot open input` on stderr, exit status 1 |
| Width `0`, `9`, or `2x` | `width must be 1..8` on stderr, exit status 1 |
| No file argument | Usage on stderr, exit status 2 |
| Width 1 versus width 2 | Same result totals; different execution order |

The checkpoint verifier includes normal, sequential, cancellation, missing-file, and invalid-width runs. Add your own fixture and use the table to predict the outcome before looking at the terminal.

## 9. Diagnose a hang using the objects you built

Temporarily remove `loop.run()` in a local copy. Our guarded result access reports `task is not complete`; it does not invent a result or manually resume the wrong coroutine. Owner destruction then unregisters pending entries. Restore the pump before continuing.

For a real suspended operation, trace these concrete links: root owner → child task → current awaiter → queue/callback registration → expected completion event. Ask which event can resume it, and whether shutdown removed that event without delivering another terminal outcome. A native stack trace alone may not show suspended coroutine relationships.

Add operation IDs, current phase, enqueue/dequeue, executor/thread identity, and terminal outcome to the prints. Never use a recycled raw address as a permanent operation ID.

## 10. Decide what must change for real I/O

The application now separates the concerns you implemented: generator/task ownership, awaiter result delivery, event-loop driving, and application policy. Replacing the simulated wait with real I/O requires the callback/registration protocol from Lesson 8, including exactly-once completion, result synchronization, cancellation, and late-callback lifetime.

An executor transition may move execution to another thread. Document whether each await preserves its executor or migrates. UI access and thread-local state must follow that contract. Release ordinary locks, borrowed buffer leases, and transactions before an unbounded suspension unless the resource explicitly supports it. Immediate completion must also work: it can run continuation code inside a call that was expected to “just start” an operation.

Measure before optimizing: outstanding frames, allocation rate, retained bytes, queue operations, ready-fast-path frequency, migrations, and end-to-end latency under load and cancellation. Compare debug and optimized builds as in Lesson 4. Frame size and coroutine count alone do not establish performance. Batching, reducing state retained across waits, pooling measured allocation hotspots, and executor affinity may help specific workloads.

The workshop task is an instructional type. It has explicit single-consumer and one-thread assumptions, no built-in asynchronous file API, no general `when_all`, and no automatic context propagation. When studying a production library, trace one operation through its task, scheduler, cancellation, and I/O adapter together. Use the ownership and completion questions you have now answered with code.

**Final exercise:** add a reported average for accepted readings, with an explicit empty-input behavior. Then introduce a larger file and compare widths. The output should remain deterministic in its totals even when scheduling order changes. Do not claim a throughput improvement from this simulated single-thread queue; a real service benchmark is a different experiment.

```bash
python3 tools/check_labs.py 12
python3 tools/check_labs.py --sanitize
```

Return to the [workshop map](00_ROADMAP.md) for the concept coverage and checkpoints you have completed.
