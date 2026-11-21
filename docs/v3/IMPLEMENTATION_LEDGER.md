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
| V3-01 | Fair consumed-resource benchmark and falsification | OPEN | Must run CLI, independently validate complete outcomes and reject cap gaming |
| V3-02 | Orchestrator extraction and typed control-plane | OPEN | v1/v2 golden replay mandatory |
| V3-03 | Fresh matched-seed adaptive discovery replication | OPEN | Real worker & holdout leakage tests mandatory |
| V3-04 | Legal cell / axis portfolio | OPEN | Exhaustive tiny-domain and huge virtual-space memory checks |
| V3-05 | Predictive cost/uncertainty acquisition | OPEN | Frozen development-only calibration |
| V3-06 | Optional surrogate challenger | NOT_REQUIRED_YET | Killable research branch; only promote if its evidence wins |
| V3-07 | Boundary taxonomy & shape selection | OPEN | Independent multi-cliff / smooth truth |
| V3-08 | Campaign statistical closure | OPEN | Population-level FWER with CI and protocol |
| V3-09 | Witness/recovery containment | OPEN | Independent replay and fault injection |
| V3-10 | External matched competition | OPEN | Reproducible pinned external runs |
| V3-11 | Release/SOTA verdict | OPEN | Exact-SHA all gates and owner merge approval |

No code file or benchmark number is self-certifying. States mean VERIFIED only after observed execution on the exact revision; PARTIAL/OPEN otherwise. 
