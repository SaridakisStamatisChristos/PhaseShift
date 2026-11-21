# PHASESHIFT v2 — Benchmark Protocol (partial)

Source plan: `PHASESHIFT_V2_SOTA_UPGRADE_MASTER_PLAN.md`, benchmark scenario inventory `benchmarks/v2/scenarios.json`.

## Existing executed or executable control
`benchmarks/v2/equal_budget.py --binary ./build/phaseshift --out /tmp/frozen-phaseshift-bench --seeds 31,47,83 --cases 20`

It runs **real PHASESHIFT worker/SQLite/confirmation** for both original `adaptive_partition` and opt-in `hierarchical_boundary_v2` on the same planted calibration threshold (24), 64 ordered points, selected-case budget and wall deadline. It collects actual worker CPU nanoseconds, host exploration/confirmation wall nanoseconds, valid/invalid samples, heldout counts, status, sealed confirmation count, peak worker RSS and localization accuracy per seed. It preserves every result including losses; output directory must not exist. Separate CTest runs one seed in disposable build-directory scratch.

**Important limitations:** equal selected-case budget is *not* equality of actual worker CPU or host wall budgets. Calibration operation-count ground truth is deterministic and seed-invariant, despite differing v1/v2 generator seed policies. These runs can falsify broad benefits but cannot establish real sorting/top-K/graph superiority, a globally calibrated false-positive rate, or a competitive external SOTA claim.

## Frozen future challenge
Scenario families: single/multiple cliff, smooth/no-signal, categorical, rare XOR interaction, invalid-status boundary, sorting/Top-K/graph actual workloads. Metric vector: confirmed true boundaries, localization error, false positives, complete seed-matched case count, actual CPU and wall cost, resident memory, scheduler overhead, and reproducible logical replay.

Before claiming a v2 uplift: independent training and sealed evaluation splits, >=20 independent heldout seeds where feasible, real equal wall/worker-CPU stopping conditions, null campaigns, resource status controls, external PELT/ruptures and fair performance-fuzzing comparisons on genuinely matching objectives. No such complete external campaign has yet been executed. Negative results will remain in the ledger and summary.


## Four-policy cliff/null frozen challenge

`python3 benchmarks/v2/policy_challenge.py --binary ./build/phaseshift --out /tmp/phaseshift-four-policy --seeds 31,47,83 --cases 20`

Tests the v1 adaptive control and v2 uniform, stratified and hierarchical search on both a planted single-cliff calibration and a constant null, with every seed/policy retained. Writes a SHA-256 stamped plan before measurements, per-run SQLite/CLI evidence, CPU/wall time, localization, confirmation and false-positive status. Refuses to overwrite the output directory. CI runs one seed as a real-worker smoke. Equal case ceilings **do not** equalize CPU/wall budgets; no external SOTA or population error-rate claim is justified.
