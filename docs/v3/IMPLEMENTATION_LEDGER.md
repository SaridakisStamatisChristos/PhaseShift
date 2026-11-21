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
| V3-02 | Orchestrator extraction and typed control-plane | PARTIAL | Legacy v2 selection extracted to a dedicated adapter with differential characterization CTest; typed keys, committed-only snapshot and budget ledger present. Full coordinator extraction and exact-SHA gates still outstanding |
| V3-03 | Fresh matched-seed adaptive discovery replication | PARTIAL | Deterministic paired discovery planner supports distinct structural keys; worker scheduling, persistence, replay and frozen holdout integration outstanding |
| V3-04 | Legal cell / axis portfolio | PARTIAL | Standalone opt-in ordered-axis slice policy, legal mixed-space contexts, deterministic reserve and 2.048B virtual-index memory/feasibility oracle written; no main CLI wiring or held-out policy benefit established |
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

## Continuation checkpoint (21 November 2026; branch head `6756d82182efa838b960486223716b22be7ed403`)

- `793acb69`: V3-02 extracted the immutable v2 selector creation, feedback, RNG snapshots and acquisition origins into `v3/legacy_policy_adapter.hpp`. The coordinator still owns Case generation, Store, worker execution and full confirmation. Added independent direct-v2 differential tests for uniform/stratified/hierarchical/cliff/hybrid paths, invalid proposals and RNG/origin continuity. **Status:** implemented, exact-SHA CI pending.
- `6756d821`: V3-04 introduced `v3/axis_slice.hpp` as a **standalone, not yet CLI-wired** bounded ordered-axis search challenger. Uses the existing disjoint legal-space coordinate index, explicitly freezes nominal coordinates rather than ranking them, reserves every fourth proposal for unbiased global candidates, caps active contexts/axis cardinality/proposals and accepts discovery-only committed feedback. Added exhaustive tiny-domain test, deterministic replay, missing/NaN/holdout rejection, and a virtual space of 2,048,000,000 legal candidates without eager materialization. **Status:** implemented, exact-SHA CI pending.
- V3-03 `replicate_planner.hpp` still supplies deterministic matched-seed **planning only**. Atomic two-worker measurement, schema provenance and resume-safe pair replay remain **OPEN**.
- No new v3 acquisition policy is enrolled in `explore`. No external matched comparators, no trained cost/surrogate model, no statistical calibration verdict. **SOTA unverified.**

### Gate discipline
CMake has native C++ and Python adversarial tests. Requiring the final head's Release GCC/Clang CTests, ASan/UBSan, real-worker diagnostic, coverage ≥95%, historical byte-for-byte replay and seeded policy trials remains mandatory. Earlier successful workflow runs do not transfer to later commits. Never merge or release without explicit owner approval.

## Adversarial verification / repair checkpoint (2026-11-21)

- On revision `793acb69`, GCC and Clang completed 97 CTests: 96 PASS, **1 FAIL** (`v3_real_worker_consumed_budget_scorecard`). Direct selector characterization passed. The `v3_axis_slice_exhaustive_legal_and_large_sparse` test also passed on later `6756d821` (97/98 CTests passed on GCC; only the same benchmark verifier failed). These are earlier-revision results, not current-head acceptance.
- **F-010 diagnosis:** V3-01 had stored a SHA-256 of raw `run.sqlite` bytes, while SQLite WAL committed pages could later checkpoint. This produced an unstable file hash and could exclude committed pages from a raw main-file hash. The failure was in the evidence-verification protocol rather than the algorithm engine.
- **F-010 repair:** `56ea0b71` replaces raw-file SHA with a read-only SQLite pager-backed backup snapshot digest, adds an independent WAL checkpoint stability regression, and preserves the source `run.sqlite` untouched. Earliest invalidated phase: V3-01 integration/verification contract; affected modules: `benchmarks/v3/protocol.py`, `run_internal.py`, `tests/v3/benchmark_fairness.py`, `fairness_contract.md`.
- **F-011 hardening:** `49773d24` makes the new axis-slice acquisition reject a finite timing metric when the observation status is `wrong_answer` (or another non-`ok`), and rejects sealed confirmation feedback; test added. No v1/v2 source changed for this fix.
- **Latest code SHA:** `56ea0b71f082c33dee893e97c747f50397d90beb`. Full exact-head CI and independent 20-seed evaluation pending verification. No new scientific-performance comparison claim or merger authority.


## V3-03 worker-backed pilot continuation

Standalone opt-in `phaseshift_v3_pairs` executable, v10 authoritative discovery-only `run.sqlite` with atomic two-sided raw worker results, deterministic seed and order replay, input-byte tamper checks and pre/post-commit forced-kill recovery. New CTest `v3_paired_discovery_real_worker_atomic_resume`. Details and limitations in [PAIRED_DISCOVERY_PILOT.md](PAIRED_DISCOVERY_PILOT.md).

**Evidence state:** code authored; new-revision CI pending. V3-03 **PARTIAL**: fixed endpoints, one axis; no adaptive replicate-vs-explore, general sealed campaign holdout, complete attempted-work budget accounting or statistical performance claim.
