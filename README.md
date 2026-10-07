# C++20 Coroutines: Learn by Building

Start with a function containing only `co_return`. Compile it. Read the error. Add the missing piece and compile again. Keep following that cycle until you can build a generator, an event loop, composable tasks, and a small file-processing application.

All 12 lessons follow a practical sequence: **write → predict → compile/run → explain the observation → change one thing → repeat**. Compiler errors are part of the course. So are programs that compile but do not start, do not resume their parent, or expose a wrong lifetime assumption.

## Begin here

You need ordinary C++ experience with classes, RAII, references, moves, templates, and exceptions. You do not need prior coroutine knowledge. Use a compiler and standard library supporting C++20 `<coroutine>`. Python 3 runs the optional checkpoint verifier; CMake provides a combined build and check workflow.

Run commands from the repository root:

```bash
mkdir -p scratch
clang++ -std=c++20 -Wall -Wextra -Wpedantic labs/01/01_void.cpp -o scratch/hello
```

**This first command is supposed to fail.** Open [Lesson 1](01_what_are_coroutines.md) and fix the program one requirement at a time. Compiler wording and the order of simultaneous diagnostics can differ.

Keep your work in `scratch/`, which Git ignores. `labs/01` through `labs/12` contain saved checkpoints for recovery and comparison. Lessons say whether a checkpoint should fail compilation, print a surprising result, or demonstrate a repair. Unsafe lifetime examples are labelled and are never executed by the verifier.

## The twelve lessons

1. [Make `co_return` compile](01_what_are_coroutines.md)
2. [Pause, resume, and retain local state](02_why_coroutines.md)
3. [Trace creation and cleanup](03_coroutine_lifecycle.md)
4. [Repair lifetime assumptions](04_coroutine_frame.md)
5. [Build an awaiter](05_suspension_resumption.md)
6. [Adapt an awaitable](06_coawait_mechanics.md)
7. [Pull values from a generator](07_coyield_generators.md)
8. [Arrange the next resume](08_creating_awaitables.md)
9. [Compose result-bearing tasks](09_creating_coroutine_types.md)
10. [Repair queued-handle ownership](10_coroutine_handle.md)
11. [Handle errors and cancellation](11_exception_handling.md)
12. [Build a bounded reading report](12_practical_patterns.md)

Work through them in order: later lessons reuse the types you built earlier. The [roadmap and coverage map](00_ROADMAP.md) connects each concept to its experiment, including the deeper lifetime, allocation, cancellation, and execution-context details.

## Check your progress

```bash
python3 tools/check_labs.py 01
python3 tools/check_labs.py
python3 tools/check_labs.py --sanitize
```

The checker verifies compiler rejection for intentionally invalid files and exact output/exit status for runnable checkpoints. It matches diagnostic concepts rather than compiler-specific full messages. `--cxx g++` selects another compiler; `CXX` is also supported. Sanitizer mode requires compiler/platform support for AddressSanitizer and UndefinedBehaviorSanitizer. It checks the runnable programs, not the deliberately unsafe fragments discussed in the prose.

To build the final application and the original compact reference examples, then check all lessons:

```bash
cmake -S . -B build-workshop
cmake --build build-workshop
ctest --test-dir build-workshop --output-on-failure
./build-workshop/workshop_report labs/12/readings.txt 2
```

The final application reads a real file, limits outstanding records, validates input, and collects results and cancellation. Its service wait is simulated with a single-thread queue; the file reads are synchronous. Each primitive's assumptions are stated where it is built.

The original [`examples/`](examples/) programs remain compact supplementary references. The learning path uses [`labs/`](labs/README.md), whose implementation evolves between lessons. The educational types are not a production coroutine runtime.

## AI Assistance Disclosure

This tutorial reflects my own learning goals, thoughts, and ideas about C++ coroutines. I used AI/LLM tools to help generate and reshape drafts, organize ideas, refine explanations, expand coverage, and review the material.

The material is intended for educational purposes. Code examples and technical explanations were reviewed and tested, but readers should verify important details against the C++ standard and the documentation of any libraries they use.
