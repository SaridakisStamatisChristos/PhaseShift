# PHASESHIFT

**CPU-only C++20 experimental algorithm behavior explorer.** PHASESHIFT creates deterministic structured inputs, checks algorithms against independent oracles, times them in a child process, retains raw observations in SQLite, and seeks reproducible structural boundaries. Its synthetic threshold is a *calibration fixture*, not a novel performance finding.

## Requirements and build

Linux x86-64, CMake >=3.24, C++20 compiler (tested GCC 14), libsqlite3-dev, libjson-c-dev, libssl-dev, pkg-config. There is no GPU, paid API, or Python runtime dependency.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/phaseshift doctor
./build/phaseshift demo --out runs/demo
./build/phaseshift explore --config configs/sorting_quicksort.json --out runs/sorting
./build/phaseshift report --run runs/demo --format markdown,json,csv,html
```

A demo run is intentionally **not overwritten**; choose a new `--out` folder for repeat experiments. CTest uses a controlled disposable test directory.

## Commands

```
phaseshift --help
phaseshift --version
phaseshift doctor
phaseshift list families
phaseshift list algorithms
phaseshift validate --config configs/topk_fraction.json
phaseshift demo --out runs/demo
phaseshift explore --config configs/graph_density.json --out runs/graph
phaseshift confirm --run runs/graph --hypothesis H1
phaseshift minimize --run runs/demo --hypothesis H1
phaseshift replay --run runs/demo --case <64-character-case-id-or-prefix>
phaseshift report --run runs/demo --format markdown,json,csv,html
phaseshift export --run runs/demo --out runs/demo_bundle
phaseshift compare --a runs/graph --b runs/graph_2
```

Use `jq -r '.raw_samples[0].case_id' runs/demo/report.json` to extract a replayable ID, or consult `samples.csv`. `report` always regenerates all four formats; `--format` is accepted for compatibility. Inputs and their SHA-256 are embedded in `run.sqlite` and validated on replay. Generated runs are ignored by Git.

## Families and algorithms

| Family | Trusted oracle | Comparators |
|---|---|---|
| `integer_array` | stable ascending sort | `std_sort`, `mergesort`, `insertion_sort` (n<=4096), `quicksort_first_pivot` (teaching pathology; n<=20000) |
| `topk` | descending full sort with duplicates | `topk_full_sort`, `topk_heap`, `topk_nth` |
| `graph` | Bellman-Ford | `dijkstra_heap`, `dijkstra_dense`, `zero_one_bfs` (0/1 weights only) |
| `calibration` | independently computed work formula | `calibration` |

`plugin:/absolute/path/to/library.so` is supported in a worker for the integer-array semantic contract. Only trusted native libraries should be used: process separation is **fault containment, not a security sandbox**. See `docs/plugin_api.md`.

## What claims the engine can make

The deterministic fixture changes declared work counts between `x=7` and `x=8` for threshold 8. It tests that a discrete segmented model can find that injected boundary and that independent-seed holdout reproduces it. The negative fixture intentionally has constant work and must abstain. Real sorting, Top-K, and graph runs report timings and correctness, but the current implementation **does not issue statistically confirmed real timing boundaries** until paired-case 99% uncertainty estimation and multiplicity control are implemented. This is deliberate fail-closed behavior.

No forced performance claims. An effect that varies with machine state is not a proof of asymptotic complexity or a causal mechanism. Reports distinguish `HYPOTHESIZED`, `NOT_CONFIRMED`, `CONFIRMED` (synthetic work-count only), `MINIMIZED_CONFIRMED`, and `NO_CONFIRMED_BOUNDARY`.

## Current limitations (v0.1)

This is a **verified functional vertical slice, not full spec v1.0**. Adaptive order is currently deterministic space-filling, *not* a feedback-optimized scheduler. There is no interrupted-search resume; discovery uses a 1D segmented-mean BIC fit and abstains from multi-axis confounded inference. CPU timing lacks paired bootstrap/FDR confirmation, and no empirical scaling fingerprint or automated fair search ablation is included. Graph generators implement directed edges, no self-loops/parallel edges; community/degree-skew morphology is pending. Input hashes are stable but generator versions are not separately migratable. The worker is spawned per case (not persistent across cases); timeout and address-space controls are best effort on Linux. Witness minimization supports confirmed synthetic parameter thresholds, not structural `ddmin` across arrays/graphs. Export emits a portable bundle but not a verified import command. Reporting is static with no dashboard. See `docs/release_checklist.md` for acceptance gap tracking.

## License

No repository license has been granted or selected. All rights remain with the repository owner, subject to the licenses of external system dependencies. A license must be explicitly chosen before redistribution under a defined license.
