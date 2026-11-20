# PHASESHIFT v2 — R7 implementation ledger

Baseline main: `93b47590c7e44eaf062d97995e9c5cd33274a7a2` on 2026-11-21. Existing push CI `build-test`, `sanitizers` and `production-line-coverage` succeeded on **this original main SHA only**.

## Hard invariants

Preserve historical generator bytes/IDs, v1 CLI behavior, SQLite v7 evidence, native plugin ABI, independent correctness oracle, sealed holdout, worker containment, and >=95% LLVM production line coverage. Use explicit v2 opt-in. Do not claim new inference or SOTA without executed equal-budget evaluation.

## R7 requirement / claim ledger

| Requirement | State | Evidence or gap |
|---|---|---|
| G0 main pinned | VERIFIED | SHA above and successful original push CI |
| M0 frozen SOTA benchmark contract | PARTIAL | Scenario inventory only; equal-budget runs outstanding |
| M2 lazy mixed-radix index and uniform sparse sampler | PARTIAL | Opt-in uniform_v2 CLI+resume added, CI verification pending; independent per-coordinate replicate cohorts added to uniform_v2 (not adaptive); conditional subspaces remain unimplemented |
| M1 explicit policy version | PARTIAL | v1 default, v2 opt-in; cached evidence/budget ledger pending |
| M5 multi-change model | PARTIAL | Exact K-bounded DP and brute-force differential test; opt-in multi_change_v2 now emits ranked discovery-only candidate events and selects best H1 for fresh confirmation; no PELT, noisy held-out validation or campaign correction |
| M3 hierarchical boundary policy | PARTIAL | Opt-in single ordered-axis midpoint prioritization, contrast/cost heuristics, deterministic exploration/replay and persisted selection origin; no calibrated uncertainty, independent paired replication, mixed interactions or held-out superiority |
| M7 multi-hypothesis correction | PARTIAL | Holm FWER fixed-family primitive + reference/negative tests; campaign sealing and selection-family persistence not implemented |
| M10 frozen calibration comparison harness | PARTIAL | Added independently executed two-policy CTest smoke, fixed seed/budget, actual worker CPU and wall logging, negative results preserved. Not equal CPU budgets, not sufficient repeats, no external SOTA |
| M4 controlled two-way interaction | PARTIAL | Independent 2x2 matched-seed difference-in-differences and status contrast; explicit invalid/partial abstention; opt-in v2 emits an immutable discovery event only, not independent confirmation or higher-order interactions |
| M6, M8–M9, M11–M12 | UNVERIFIED | Implementation pending |
| G1–G14 v2 hard gates | UNVERIFIED | No branch CI or held-out comparison yet |

Research claim: **none**. Any synthetic fixture is a correctness calibration, not scientific novelty. No PR merge, license or release is authorized.

## CI falsification / repair F-v2-001

Commit `1e3cf349` ran 39/39 CTests successfully in the LLVM coverage job, but the coverage gate **failed**: `src/boundary_v2.cpp` was missing from the instrumented linked object set. Earliest affected phase: INTEGRATE (coverage aggregation), not the detector algorithm. Repair: explicitly include the executable that links the new translation unit in `llvm-cov export`; rerun coverage on the repaired branch SHA. This is not an accepted pass until exact-revision CI reports success.

## Workflow hardening

The built-in equal-case-budget comparison is an **ephemeral CTest scratch benchmark**. The wrapper cleans only its own CMake build-directory fixture before rerun. Real benchmark evidence created by the direct `equal_budget.py --out ...` invocation is immutable and the tool refuses to overwrite it. `multi_change_v2` retains any v1 correctness/resource/crossover boundary in preference to a weaker timing-only segmentation.
