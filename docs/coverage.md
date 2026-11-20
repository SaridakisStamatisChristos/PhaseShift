# PHASESHIFT production coverage gate

**Target:** >=95% executable **line coverage**, measured using Clang/LLVM
source instrumentation over all C++/C translation units under `src/` and
`apps/`. These are the engine and both CLI entry points. The gate does **not**
claim 95% branch, MC/DC, state-space, oracle or mathematical proof coverage.

Run from the checkout:

```bash
bash scripts/coverage.sh
```

Dependencies: clang/clang++, llvm-cov, llvm-profdata, CMake, Ninja (optional),
SQLite3/json-c/OpenSSL development libraries, Python 3 for result parsing.
The script instruments all relevant production executables, clears stale raw
profiles, runs every CTest (including spawned workers), merges the actual
profiles, and fails if any production source translation unit is missing from
the report or if the aggregated percentage falls below 95%. It prints source
files and a separate, explicitly ungated branch coverage total. CI uses the
same script on an Ubuntu Linux runner.

`scaling_independent_holdout_knee` exercises a fully real worker/SQLite/scaling
pipeline using an **intentionally synthetic latency transition** provided by
an oracle-correct sorting plugin; it is a test of detection/confirmation
implementation, not evidence that real algorithm runtimes are hardware
invariant or that PHASESHIFT discovered a new asymptotic law. The test requires
both a confirmed hypothesis from 20 fresh seed cohorts and independent size
provenance checks. Controlled sleep can be affected by extreme host scheduling
noise; on a failed runner investigate the timing profiles instead of relaxing
statistical acceptance thresholds.

A coverage percentage is a code-execution metric, not a substitute for the
existing independent correctness or scientific-evidence acceptance tests.
