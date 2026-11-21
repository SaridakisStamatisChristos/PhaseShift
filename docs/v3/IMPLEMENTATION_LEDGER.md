# PhaseShift v3 implementation and R7 evidence ledger

**Contract:** PHASESHIFT-V3-SOTA-ARCH-001, 21 November 2026. Scope: offline C++20 core; Python only for standalone benchmark/adapters.
**Immutable baseline:** `279a8c65ad7ec3741a727ff49aa7f84e0628a6f1`, merged PR #5. GitHub main HEAD and three push Actions success runs independently inspected on 21 November 2026. See [BASELINE.json](BASELINE.json). No post-merge v2 expensive holdout run established.
**Branch:** `feature/phaseshift-v3-sota-research`. Do not merge without explicit owner authorization.

## Hard invariants / acceptance evidence

| ID | Contract | Required independent evidence | State |
|---|---|---|---|
| C01 | Preserve old v1/v2 CLI, generator and IDs | Differential frozen CLI and replay | OPEN |
| C02 | Oracle status never promoted into speed | Real worker adversarial fixtures | OPEN |
| C03 | Fresh confirmation isolation | Seed registry + leakage attacks | OPEN |
| C04 | SQLite authoritative, cache derived | Tamper/restart tests | OPEN |
| C05 | Deterministic logical proposals | Exact seeded transcript equality | OPEN |
| C06 | Atomic decisions and budget costs | Fault injection + read-only SQLite verification | OPEN |
| C07 | Bounded resources, honest accounting | Measured cost/censored and descendant tests | OPEN |
| C08 | Valid fixed-family inference | Real CLI correlated null campaigns | OPEN |
| C09 | Offline C++20, Python optional benchmark | GCC/Clang, no mandatory runtime adapter | OPEN |
| C10 | Historic evidence immutable | Backward schema/replay smoke | OPEN |
| C11 | Opt-in v3, legacy fallbacks | Default/flag differential | OPEN |
| C12 | Scoped empirical novelty only | Matched heldout/external baselines | OPEN |

## Milestones (hard gates)

| Milestone | Scope | State | Evidence |
|---|---|---|---|
| V3-00 | Live baseline, historical provenance | PARTIAL | Exact main/tree/3 push workflows matched; old fixtures not independently rerun |
| V3-01 | Fair consumed-resource benchmark and falsification | PARTIAL | Real CLI diagnostic, 20-seed separate manifests, strict SQLite/raw-log verifier and Release CTest written; broader landscapes/full heldout/CI evidence pending |
| V3-02 | Orchestrator extraction and typed control-plane | PARTIAL | Typed v3 keys, committed-only discovery snapshot, RAM budget ledger implemented; experiment.cpp extraction and golden adapters still outstanding |
| V3-03 | Fresh matched-seed adaptive discovery replication | PARTIAL | Deterministic paired discovery planner supports distinct structural keys; worker scheduling, persistence, replay and frozen holdout integration outstanding |
| V3-04 | Legal cell / axis portfolio | OPEN | Exhaustive tiny-domain and huge virtual-space memory checks |
| V3-05 | Predictive cost/uncertainty acquisition | OPEN | Frozen development-only calibration |
| V3-06 | Optional surrogate challenger | NOT_REQUIRED_YET | Killable research branch; only promote if its evidence wins |
| V3-07 | Boundary taxonomy & shape selection | OPEN | Independent multi-cliff / smooth truth |
| V3-08 | Campaign statistical closure | OPEN | Population-level FWER with CI and protocol |
| V3-09 | Witness/recovery containment | OPEN | Independent replay and fault injection |
| V3-10 | External matched competition | OPEN | Reproducible pinned external runs |
| V3-11 | Release/SOTA verdict | OPEN | Exact-SHA all gates and owner merge approval |

No code file or benchmark number is self-certifying. States mean VERIFIED only after observed execution on the exact revision; PARTIAL/OPEN otherwise. 

## Recorded v3 branch checkpoints (pending exact-head CI verification)

- `0b02559`: V3-00 baseline identity/three historical push-workflow states and R7 ledgers.
- `d347524`: V3-01 consumed-budget runner, disjoint manifests, independent scorecard and falsification unit tests.
- `4d4e02b`: V3-01 CTest real CLI and raw SQLite diagnostic added.
- `4250e55`: V3-02 typed keys, budget/admission, committed evidence; V3-03 pure deterministic paired-planning contract.
- `e9d4df2`: repair Release-build testing blind spot by replacing C++ `assert` with unconditional CHECK.

All listed test/evidence outcomes remain **UNVERIFIED** until the exact branch head passes independent CI; a previous commit's green checks cannot be transferred automatically. No claims of measured v3 superiority, complete V3-03, or release readiness.
