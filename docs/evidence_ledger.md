# Local evidence ledger — 2026-11-20

Environment: Linux 6.18.57 x86-64, GCC 14.2.0, CMake 3.31.6, AMD EPYC 9V74 (virtualized). Toolchain libraries: SQLite 3.46.1, json-c 0.18, OpenSSL 3.5.7.

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2`: **PASS**.
- `ctest --test-dir build --output-on-failure` (after clean smoke output): **PASS 4/4**, core, synthetic demo, negative control, synthetic search ablation.
- `cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPHASESHIFT_SANITIZE=ON && cmake --build build-sanitize -j2 && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure`: **PASS 4/4** after disabling RLIMIT_AS only under ASan; initial attempt failed due ASan virtual shadow reservation.
- `phaseshift explore --config configs/negative_control.json --out runs/negative`: **PASS**, 16 cases / 32 observations / 0 failures, `NO_CONFIRMED_BOUNDARY`.
- `phaseshift explore --config configs/sorting_quicksort.json --out runs/sorting`: **PASS**, 15 cases / 180 observations / 0 failures, `NO_CONFIRMED_BOUNDARY` (multi-axis confounding; not evidence no pathologies exist).
- `phaseshift explore --config configs/topk_fraction.json --out runs/topk`: **PASS**, 12 cases / 144 observations / 0 failures, `NO_CONFIRMED_BOUNDARY` (multi-axis confounding).
- `phaseshift explore --config configs/graph_density.json --out runs/graph`: **PASS**, 6 cases / 72 observations / 0 failures, `NO_CONFIRMED_BOUNDARY`.
- CTest demo initial report showed engineered threshold between x=7 and x=8, `MINIMIZED_CONFIRMED`, 72 samples including 40 independent-seed holdouts. This is a deterministic work-unit fixture only.

No claims here about remote CI, external deployment, or local experimental superiority. Generated `runs/` and `build*/` artifacts are excluded from Git.

## v0.2 additional evidence

- `ctest --test-dir build --output-on-failure`: **PASS 4/4** (including 64-bin, 24-discovery-case synthetic ablation).
- `ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure`: **PASS 4/4**.
- `phaseshift ablate --config configs/adaptive_sparse.json --out runs/ablation_sparse`: 24 discovery cases for each of grid, random, stratified, and adaptive. Grid abstained (had not reached injected threshold), random confirmed a boundary in bin [18,25] (one-bin localization error), stratified confirmed [18,24], and adaptive confirmed exact [23,24]. Each confirmed approach used 40 additional holdout cases; time and sample counts are in `ablation.csv`. This is one deterministic fixture and does not prove adaptive superiority on real workloads.
- `cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-clang -j2 && ctest --test-dir build-clang --output-on-failure`: **PASS 4/4**, Clang 17.0.0. The first Clang build exposed a sign-conversion warning; corrected the index cast and reverified.

## v1 implementation and hardening evidence — 2026-11-20

These entries supplement the original v0.1/v0.2 log above. Its earlier statements are historical and not the current feature list.

- `ctest --test-dir build --output-on-failure -j2`: **PASS 20/20** (GCC 14 Release, after structural witness, fault, morphology, statistical tests, and per-sample timeout censoring). Four further complete repeated 20/20 passes were observed after adding configurable CI/FDR policy, before the final fault-timeout test.
- `ctest --test-dir build-clang --output-on-failure -j2`: **PASS 20/20** (Clang 17 Release, same source with per-sample deadline test).
- `ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure -j2`: earlier **PASS 19/19** for structural witnesses; after adding the twentieth statistical-policy test and worker timeout case, the full invocation stopped unexpectedly at test 18 without a full suite verdict. `ctest -R 'failure_boundary_confirmation|literal_structural_witness|statistical_calibration'` subsequently **PASS 3/3** under the same sanitizer build. The first 17 of the interrupted invocation were also reported PASS, but that is **not** an observed all-in-one 20/20 suite. Until a complete rerun finishes, mark the current 20-test ASan matrix **PARTIAL**, not full PASS.
- `bash scripts/acceptance_smoke.sh`: **PASS**, evidence directory `runs/acceptance.ajLlAP3t`. Tests `doctor`, enumeration, config validation, calibrated `x=7/8` synthetic threshold, sorting/TopK/graph/negative cases, static report formats, canonical replay, SHA-256 verified bundle export/import, compatible `compare`, independent 8-size × 3-seed empirical scaling and adaptive/grid/random/stratified synthetic ablation.
- `tests/adversarial_algorithms.cpp`: **PASS**; 2,200 sorting cases, 6,600 Top-K parameter cases, 850 independent Floyd–Warshall tiny graphs and int64 boundary cases.
- `tests/ipc_negative.cpp` + `tests/tests.cpp`: **PASS**, malformed/oversize/truncated/versioned worker input, process faults, plugin OOM/ABI mismatch, state-reset contamination, invalid outputs, intermittent batch corruption, per-sample elapsed timeout.
- `tests/run_failure_boundaries.cmake.in`: **PASS**, independent 20-pair correctness and synthetic plugin-resource status boundaries at `n=1/2`, then witness minimization/replay/export. The resource case is an injected plugin return code, not proof of physical OOM.
- `tests/literal_witness.cpp` + `tests/minimization_unit.cpp`: **PASS**, byte-preserving array/Top-K and directed graph minimization, nonterminal graph vertex renumbering, invalid literal v3 schema rejection and exact SHA replay.
- `tests/statistical_calibration.cpp`: **PASS**, reference BH calculation, constant intervals, deterministic nulls, paired sign-flip, 120 deterministic synthetic null challenges and configurable policy validation.
- `tests/evidence_schema.cpp` + `tests/run_bundle.cmake.in`: **PASS**, SQLite schema 1→6 migration retaining 72 old samples with `NULL` unavailable CPU values; case hash/config retention; bundle tamper rejection.

Unverified: GitHub Actions execution on the published commit, AArch64/macOS/Windows compilers, genuinely adversarial untrusted-plugin security isolation, and general causal or global FDR claims. The source archive/evidence paths are not universal performance guarantees.

## v1.0 acceptance closure — current implementation, 2026-11-20

These results are direct local execution observations on **the same source tree** after undirected graph witness minimization, standalone built-in reproducer generation and expanded CLI/CI instructions:

- **PASS** GCC 14.2.0 Release: `cmake --build build -j2`; CTest 1–20 **20/20**, and `ctest --test-dir build -R standalone_builtin_reproducers --output-on-failure` **1/1**. The combined uninterrupted invocation exceeded the host tool's per-call window at test 21, so the results are accurately reported as *21 distinct tests passed in two invocations*, not one uninterrupted CI result.
- **PASS** Clang 17.0.0 Release: `cmake --build build-clang -j2`; core tests **20/20** and standalone witness **1/1**. No codegen warning failures with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` on owned sources.
- **PASS** GCC Debug ASan/UBSan: `cmake --build build-sanitize -j2`; CTest test IDs **1–10: 10/10**, **11–20: 10/10**, **21: 1/1**, each executed with `ASAN_OPTIONS=detect_leaks=1`. All 21 distinct sanitizer tests completed with zero failures in three batches. This replaces the earlier *PARTIAL* historical suite note above, without changing the original record of interrupted runs.
- **PASS** `bash scripts/acceptance_smoke.sh`: retained under `runs/acceptance.4IqbEFDl`. Synthetic 7/8 fixture confirmation, negative control, sorting/Top-K/graph correctness, independent scaling, grid/random/stratified/adaptive ablation and exact SQLite export/verify/import/replay passed. No invalid samples were reported in the real-algorithm acceptance cases.
- **PASS** structural witness suite: undirected logical-edge pair ddmin, identical reverse-weight verification, no self loops/parallel pairs, vertex renumbering, frozen oracle agreement and replay.
- **PASS** standalone witness suite: `phaseshift reproduce` generated compilable no-dependency C++20 source for all twelve built-in algorithms; generated programs matched the frozen oracle result. Tests also covered the minimum and maximum signed 64-bit values, empty Top-K, calibration work count and refusal to fabricate plugin implementation source. The exported single-call latency is diagnostic only.
- **PASS** reproducible remote commit objects: GitHub API created trees with SHA exactly matching locally staged Git tree objects; branch updates were guarded against concurrent HEAD changes, then read back to confirm the new SHA.

**External gates still open:** GitHub Actions runner execution for the final commit is not observed in this document; cross-hardware/x86 versus AArch64 portability and untrusted plugin sandboxing are not certified. The owned v1.0 executable feature set has passed *local* acceptance, not universal production deployment.
