# Workshop checkpoints

These files accompany the twelve lessons in the repository root. Begin with the lesson, type the small changes yourself, and use these files to compare or recover a checkpoint.

The number prefix orders the checkpoints within a lesson. Headers beside them are the implementation at that stage. Later lessons deliberately replace earlier policies: use Lesson 10's unregistering loop with the final application, for example. The early Lesson 8 loop requires owners to remain alive until it drains.

Do not compile every `.cpp` in a glob: some files are intentionally invalid. Read the associated lesson or use the checker, which knows the intended outcome.

```bash
python3 tools/check_labs.py 01
python3 tools/check_labs.py
python3 tools/check_labs.py --cxx clang++ --sanitize
```

Run these commands from the repository root. `CXX` can choose a compiler too. The checker builds in a temporary directory, removes its binaries afterward, verifies relevant diagnostic terms for compile failures, and compares exact stdout, stderr, and exit status for runnable programs. Compiler diagnostic wording and simultaneous-error ordering may vary.

| Lesson | Checked scenarios | Expected compiler rejections |
|---|---|---|
| 01 | 7 | 6 |
| 02 | 4 | 0 |
| 03 | 2 | 0 |
| 04 | 3 | 0 |
| 05 | 5 | 3 |
| 06 | 5 | 2 |
| 07 | 3 | 1 |
| 08 | 3 | 0 |
| 09 | 5 | 2 |
| 10 | 3 | 1 |
| 11 | 4 | 1 |
| 12 | 5 | 0 |

The final application has multiple scenarios using the same source. The [manifest](checkpoints.json) is the authoritative list of expected results. All regular runnable checkpoints avoid undefined behavior. Unsafe fragments are identified in the lesson prose for reasoning or optional local sanitizer investigation.

The callback adapter and queue are single-threaded teaching implementations. Callback inline/deferred/duplicate/late behavior is exercised explicitly. Threaded completion, real asynchronous I/O, and production timeout machinery require additional protocols explained in the lessons.
