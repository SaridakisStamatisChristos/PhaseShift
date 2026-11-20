# PHASESHIFT — v1 acceptance checklist (2026-11-20)

This ledger distinguishes a **locally verified executable v1 candidate** from a blanket cross-platform, research-grade or remotely CI-validated release. Contract: [`PHASESHIFT_SPEC.md`](PHASESHIFT_SPEC.md), particularly G0–G6 and the reproducibility/negative-control requirements.

| Gate | Locally observed result | Remaining limitations / external evidence |
|---|---|---|
| G0 repository and dependency safety | **PASS**: repository baseline inspected, CPU-only C++20, dependency licenses not replaced; no GPU or API requirement | no repository-wide user-selected license; normal system dependency provisioning required |
| G1 algorithm and morphology correctness | **PASS**: 2,200 deterministic sorting tests, 6,600 Top-K tests, 850 independent Floyd–Warshall graph comparisons; additional extreme signed values, duplicate/tie, zero-one restrictions | external platform/endian behavior not independently tested; full graph structural distributions do not imply every possible multigraph policy |
| G2 worker correctness and fault containment | **PASS**: dedicated executable, bounded versioned binary IPC, per-sample wall censuring, group kill/reap, balanced order, output-by-output oracle checks, error/timeout/OOM/ABI tests | trusted native plugins only; no seccomp sandbox; Linux limits differ under ASan; persistent worker lives per case (not across whole experiment) |
| G3 detection, calibration, and confirmation | **PASS for declared single-hypothesis design**: deterministic threshold and null, conditioned multi-axis slices, fresh seed 20-pair holdout, 99% bootstrap, matched sign-flip and configurable BH-FDR q, correctness/resource-status boundaries | no simultaneous multiple-hypothesis family across independent runs; no global FDR guarantee; scaling model fits do not automatically constitute a confirmed regime change |
| G4 minimization and replay | **PASS for tested paths**: parameter and literal structural reductions for arrays/Top-K/directed and undirected graphs with graph-vertex renumbering; independently validated wrong-answer/status witness; empirical timing minimization fails closed; legacy migration and witness replay | standalone source embeds at most 2 MiB of input; larger cases and external plugins use verified input + runner bundles; minimization can legitimately return provisional/no reduction |
| G5 reproducibility/ablation | **PASS locally**: deterministic scheduler resume, schema v6 migrations, raw/aggregate data, audited export/import and tamper rejection, four-method equal-case-budget synthetic and real-workload ablations, scaling holdout | no independent cross-hardware comparability measurements; actual CPU time cost is never guaranteed equal across search methods |
| G6 build/CLI validation | **PASS locally**: GCC 14 Release, Clang 17 Release, GCC Debug ASan/UBSan builds, each with 21 distinct tests passing; CLI end-to-end acceptance, reports/replay/export/import/compare, static reports | GitHub Actions run outcome has not been observed; Linux AArch64/macOS/Windows were not tested; no production/security/SOTA certification |

## Required caveats

- The synthetic work-unit boundary between `x=7` and `x=8` is intentionally *engineered*, not an algorithmic discovery. Its successful detection is only calibration evidence.
- A fixture that injects `out_of_memory` in a plugin verifies **status-handling**, not actual host memory exhaustion. Fault injection is not evidence of a real hardware boundary.
- Real sorting/Top-K/graph experiments may give `NO_CONFIRMED_BOUNDARY`, `HYPOTHESIZED` or `NOT_CONFIRMED`; no numerical speedup is promised.
- Statistical inference is limited to the declared, single frozen hypothesis and independently held-out cases. The engine does not guarantee calibrated error under unlimited repeated exploration or unknown nonstationary loads.
- A reported test **PASS** applies only to the indicated checked code, toolchain and test environment. CI YAML alone does not prove CI execution.

**Release label**: `PHASESHIFT v1.0 core acceptance complete locally on Linux x86-64; GitHub Actions and other platform qualification are pending evidence`. Don't mark production-ready or mathematically proven.
