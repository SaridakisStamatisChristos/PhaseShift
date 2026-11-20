# PHASESHIFT

**A CPU-only, offline C++20 instrumented experiment engine for discovering and verifying structural behavior boundaries in algorithms.** Its output is a replayable, provenance-linked observation, not an assertion of a new mathematical or causal law.

PHASESHIFT **v1.0 core passed 25/25 GCC, Clang and ASan/UBSan CTests plus three CLI smoke runs on GitHub Actions** at previous `main` commit `326863e92c6e` ([build-test](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44744221461), [sanitizers](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44744221259)). The subsequent independent R7 audit revealed scientific-evidence and interface defects. Post-audit repairs add regression gates; **the revised commit `6e343b8f1efe` passed GCC, Clang, ASan/UBSan and sanitized CLI acceptance on GitHub Actions: 31/31 tests per toolchain** ([build-test](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44756583264), [sanitizers](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44756583308)). See [`docs/r7_audit_repair_2026-11-20.md`](docs/r7_audit_repair_2026-11-20.md).

## Build on Linux

Install CMake >=3.24, Ninja (recommended), GCC >=12 or Clang >=16, C/C++20 headers, SQLite3 development libraries, OpenSSL Crypto development libraries, `json-c` and `pkg-config` (Ubuntu: `cmake ninja-build g++ clang libsqlite3-dev libjson-c-dev libssl-dev pkg-config`). No GPU, Python interpreter, paid API, hosted model or Internet connection is required to execute experiments after dependencies have been installed.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/phaseshift doctor
./build/phaseshift demo --out runs/first_demo
```

Or use `cmake --preset portable-release`, `cmake --build --preset portable-release`, `ctest --preset portable-release`; other presets cover Clang and ASan/UBSan. No implicit `-march=native` is enabled. Run `bash scripts/acceptance_smoke.sh` to execute the natural command-line acceptance protocol; it needs `jq` only for shell-level assertions and writes a new preserved directory under `runs/`.

## Built-in algorithms and inputs

| Family | Oracle | Tested algorithms |
|---|---|---|
| `integer_array` | independent stable ascending sort | `std_sort`, `mergesort`, `insertion_sort` (`n<=4096`), vulnerable first-pivot quicksort (`n<=20000`) |
| `topk` | descending full sort, duplicate-retaining | `topk_full_sort`, `topk_heap`, `topk_nth` |
| `graph` | Bellman-Ford, with independent Floyd–Warshall tiny-graph tests | heap / dense Dijkstra, `zero_one_bfs` only on 0–1 weighted graphs |
| `calibration` | exact instrumented operation-count formula | synthetic threshold and deterministic no-change control |

Array morphology includes uniform, ascending, descending, nearly-sorted, organ-pipe, sawtooth, alternating, all-equal, heavy-duplicate, few-unique, and Zipf-like deterministic distributions. The versioned generator supports value range, sign/duplicate/disorder fractions and run lengths. Graph parameters include directed/undirected edges, density, degree skew, communities, connectivity, source and weight classes. Generator v1 stays replay-compatible; v2 adds typed morphology; v3 is an exact literal-byte witness representation for schema-aware array/Top-K/directed-and-undirected-graph minimization.

A native trusted plugin can implement the versioned C ABI in [`include/phaseshift/plugin.h`](include/phaseshift/plugin.h); v1 plugins support the integer-array contract only. Plugins execute in a bounded child worker, not in the parent process, but **native plugin execution is not a complete security sandbox**. Malformed plugin ABI, wrong answers, exceptions, crashes, timeouts and memory failures are isolated and recorded as such, not included in valid timing rankings.

## CLI commands

```bash
./build/phaseshift --help
./build/phaseshift --version
./build/phaseshift list families
./build/phaseshift list algorithms
./build/phaseshift validate --config configs/sorting_quicksort.json
./build/phaseshift demo --out runs/demo_001
./build/phaseshift explore --config configs/sorting_quicksort.json --out runs/sort_001
./build/phaseshift resume --run runs/sort_001
./build/phaseshift confirm --run runs/sort_001 --hypothesis H1
./build/phaseshift minimize --run runs/demo_001 --hypothesis H1
./build/phaseshift reproduce --run runs/sort_001 --case CASE_ID --algorithm std_sort --out witness.cpp
./build/phaseshift replay --run runs/demo_001 --case CASE_ID --db runs/demo_001/run.sqlite
./build/phaseshift report --run runs/demo_001 --format markdown,json,csv,html
./build/phaseshift scaling --run runs/sort_001 --out runs/sort_scaling --seeds 3
./build/phaseshift ablate --config configs/adaptive_sparse.json --out runs/search_ablation
./build/phaseshift export --run runs/demo_001 --out runs/demo_bundle
./build/phaseshift verify-bundle --bundle runs/demo_bundle
./build/phaseshift import --bundle runs/demo_bundle --out runs/imported_demo
./build/phaseshift compare --a runs/sort_001 --b runs/sort_002
```

`reproduce` exports a self-contained, standard-library-only C++20 witness for built-in algorithms and embeds a frozen independent oracle answer. Generated programs are compiled and executed in CTest. Single-sample printed timings are not confirmed performance findings. Embedded inputs are capped at 2 MiB; larger cases require the verified input + runner evidence bundle. Native plugins cannot be converted to source because their code is not owned by this project.

`resume` resumes an unfinished experiment by replaying the transactional scheduler decision log; complete runs cannot be silently overwritten. `confirm` requires a frozen, detected candidate; **it is expected to reject the command for an experiment that reports `NO_CONFIRMED_BOUNDARY`**. `minimize-case --run RUN --case CASE_ID --algorithm plugin:/absolute/path.so` separately shrinks an oracle-confirmed wrong answer. `replay ... --expect wrong_answer --algorithm plugin:...` verifies expected incorrect plugin behavior. The `compare` command rejects incompatible algorithm contracts and warns about hardware equivalence; it is not a causal performance comparison.

## What is and is not confirmed

The known synthetic fixture crosses its engineered threshold from `x=7` to `x=8` (for `threshold=8`). Its `SYNTHETIC_CALIBRATION` result is confirmed with independent cases and a byte-replayable witness. The constant-work negative control must return `NO_CONFIRMED_BOUNDARY`.

For real algorithms the engine records balanced ABBA / BAAB execution order, per-operation wall and CPU timing, peak memory and failure outcomes. Search methods include grid, uniform random, stratified and feedback-adaptive numeric-axis selection; each has a deterministic, budgeted schedule. A single selected and **frozen** performance candidate can be independently checked using new case seeds, case-paired bootstrap confidence intervals (99% default), a sign-flip randomization test and declared BH-FDR policy. The practical-effect ratio, confidence and FDR q are configurable. A failed confirmation remains `NOT_CONFIRMED`; a noisy performance witness becomes provisional, not silently promoted. Categorical structural axes can be conditioned on, but are **not falsely interpreted as ordered numeric axes**.

Operation-count calibration is not real performance; fitted scaling functions are empirical over finite sizes, not Big-O proofs; observational boundaries alone are not causal. Performance rankings never consider wrong-answer, crash or timeout samples valid.

## Evidence and reproducibility

Each run contains a versioned SQLite database with raw measurement rows, environment and algorithm records, scheduler transcript, generator identity, hypotheses, confirmations, witnesses and events. `report.json`, `report.md`, `samples.csv` and `report.html` are static local projections. `phaseshift export` hashes an immutable bundle and verifies its input bytes, manifest and database; `verify-bundle` rejects tampering, and `import` refuses to overwrite an existing destination. Original raw evidence is never overwritten by replay. Normal generated runs and builds are Git-ignored.

The benchmark/host provenance, significance limitations, acceptance test matrix, safety constraints and prior-art comparisons are described in `docs/`.

## Licensing

Native v1 plugin artifacts receive a SHA-256 binary identity in the algorithm evidence registry when the plugin file is available. A changed native binary under the same path is rejected rather than silently mixed into the run. External plugin build flags are not recoverable from this ABI; no metadata is fabricated.

No project license has been selected; the repository owner retains applicable rights. System dependencies have their own licenses. Do not redistribute PHASESHIFT under an invented license.

## R7 v1.0 contract hardening

The original v1.0 specification has an explicit requirement-to-evidence map in [`docs/v1_requirements_evidence.md`](docs/v1_requirements_evidence.md). In addition to the original functionality, current source provides configurable adaptive exploration fraction and measured batch floor, a fresh-worker-per-sample option, detailed host/build provenance comparison, irreversible experiment evidence state transitions, empirical scaling-regime detection with new-size holdout confirmation, and matched-seed structural interventions. These features are intentionally conservative and may abstain.

Config excerpts (both configuration forms persist into the immutable run manifest):

```json
{
  "search": {"method": "adaptive_partition", "exploration_fraction": 0.20},
  "measurement": {"minimum_batch_ms": 2, "isolation_mode": "per_case"}
}
```

`isolation_mode: "per_sample"` starts one child process per timed operation and preserves paired blocks/order metadata. It is slower and measures a different cache/plugin-state regime than `per_case`; compare results only after documenting this difference. `minimum_batch_ms` has a 1–100 ms range, and the adaptive exploration fraction must be between zero and one inclusive. The scheduler's quota is deterministic, including after resume.

Use `phaseshift scaling --run RUN --out NEW_RUN --seeds 3` for empirical scaling fits. If a predeclared single-algorithm segmented candidate is identified, PHASESHIFT can check it on **new sizes and independent seeds**, applying the configured confidence and practical slope-change threshold. A model fit alone is never upgraded to a confirmed finding. Use `phaseshift intervene --run RUN --out NEW_RUN --hypothesis H1 --pairs 20` to measure matched-seed neighbor cases and enumerate observed morphology changes. This is an *intervention diagnostic*, **not a causal-identification proof**.

`phaseshift compare --a RUN --b RUN` evaluates recorded host/build metadata and emits explicit compatibility states. The code or library revision, compiler flags, CPU model/affinity/governor, and platform can differ even when two configurations name the same algorithm. Matching metadata cannot prove thermal, noise or hardware equivalence. Do not embed secrets into compiler flags intended for public evidence reports.

## Post-audit scientific evidence integrity (SQLite schema 7)

Independent R7 audit findings F-001–F-009 are tracked in [`docs/r7_audit_repair_2026-11-20.md`](docs/r7_audit_repair_2026-11-20.md). SQLite v7 now requires a `TESTED` hypothesis and a sealed, SQL-linked independent holdout cohort before `CONFIRMED`; the audit can inspect actual CaseIds and SampleIds. A legacy v6 `CONFIRMED` row without a v7 seal is retained in raw storage but projected in reports as `UNVERIFIED_LEGACY_CONFIRMATION`, not independently attested. Scaling discovery and holdout populations are distinct, and the report publishes the actual new-size set. Unknown/duplicate CLI options are rejected; replay's `--db` selects and verifies the supplied existing SQLite database instead of being ignored. Unsupported primary metrics and native plugin-family combinations fail validation.

The B07 real 0–1 graph comparison, deterministic B10/B11 synthetic research controls and B02 null-ablation scorecard are included in CTest. Per-method ablation reports discovery yield and null false positives when the ground truth is known; witness byte reduction is reported only if minimization actually ran, otherwise `NA`. Simulator successes are not physical CPU findings. The full 31-test GitHub-hosted acceptance suite was executed on commit `6e343b8f1efe` under GCC, Clang and ASan/UBSan, including all three CLI smoke tests.
