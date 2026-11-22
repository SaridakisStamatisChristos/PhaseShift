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
| V3-03 | Fresh matched-seed adaptive discovery replication | PARTIAL | v10 fixed-pair and v11 single-axis adaptive real-worker design, atomic two-sided SQLite evidence, deterministic resume; full cost and confirmation isolation gates remain open |
| V3-04 | Legal cell / axis portfolio | PARTIAL | Standalone axis slicer plus new opt-in v12 multi-axis conditional matched worker portfolio over disjoint admissible rectangles; rare-stratum screening, 2.048B sparse index, replay and tamper falsifiers. Full region tree and heldout benefit outstanding |
| V3-05 | Predictive cost/uncertainty acquisition | PARTIAL | v15 experimental committed cost acquisition; prequential split, fallback and score ablation tests; external development calibration and measured net gains pending |
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


## V3-03 adaptive discovery successor

New opt-in **schema v11** behind phaseshift_v3_pairs adaptive retains v10 fixed pair behavior and does not modify v1/v2 CLI or SQLite. The bounded adaptive_pair.hpp policy discovers distinct legal ordered-axis edges, commits discovery-only matched-seed results and uses a robust MAD ranking diagnostic after at least three valid independent seeds. It preserves exploration by reserving every fourth turn for an unsampled edge while one remains.

Every adaptive decision and both real-worker outcomes commit in one new-run transaction. Independent replay checks decisions, raw-input identities, whole worker status and numeric effect reconstruction, and predecision evidence digest. Falsification gates include OS crash before/after commit, fabricated decision arm/labels/digest/seed/effect, nonfinite and holdout rejection and an independent noise-confounding fixture.

**Evidence status:** CODE WRITTEN; exact-head CI pending. V3-03 remains PARTIAL: only one numeric ordered axis, no campaign-wide sealed confirmation registry, no full killed-worker cost recovery, no predictive calibration, no external SOTA comparison.

## V3-04 conditional paired-worker successor (schema v12)

Pure conditional_pair.hpp policy: selects legal adjacent primary-axis contrasts inside immutable non-primary coordinate contexts from an AdmissibleSearchSpace union of disjoint rectangles. One declared numeric primary; other axes may be categorical. Explicit first-pass coverage of every declared stratum precedes volume-weighted sparse candidate screening; a bounded context pool and sparse Fisher-Yates permutation prevent eager product expansion. Every fourth slot attempts a globally new context/edge; ambiguous observed edges can be remeasured with fresh matched seeds. Independent null/censor/NaN/confirmation feedback is rejected.

New conditional option in the existing opt-in phaseshift_v3_pairs executable creates a separate v12 authoritative SQLite schema. Stored full-coordinate design, arm, and predecision evidence digest bind atomic real-worker observations. Verify and resume independently replay each policy decision and raw input/worker effect, while previous v10/v11 paths and stable v1/v2 remain unchanged. To avoid a false legal-case promise, v12 currently **rejects conditional implication rules** rather than accidentally overlooking them; only disjoint admissible rectangles are supported.

Verification program: tests/v3/conditional_pair.cpp (exhaustive legal tiny oracle, guaranteed rare-stratum exposure, categorical XOR interaction isolation, malformed/nonfinite/holdout feedback, 2.048B cardinality sparse-RSS challenge); tests/v3/conditional_worker_cli.py (real workers, seeded pause/resume, before/after commit failure, raw and decision tamper, unsupported-rule rejection, fixed schema backward compatibility).

**State:** code produced; full exact-head CI evidence required before VERIFIED engineering claims. The V3-04 milestone remains PARTIAL: no generalized implication-rule frontiers, advanced region splits/shape classification, independent heldout performance proof, or external competitor comparison.

## V3-04 frozen implication-rule execution successor (schema v13)

The new `conditional-rules` opt-in mode evaluates the **existing v2 compiler's** disjoint union from the frozen source rule set (not a new parser), then submits legal mixed-dimensional contrasts to the already-tested v12 real worker pair scheduler. v13 is a separate SQLite user_version; v12's rejection of implication rules is retained to preserve historical replay identity. The new protocol is only valid for nonempty declared rules.

Added independent `tests/v3/conditional_rules_worker_cli.py`: exhaustively enumerated 16 calibration tuples with exactly six permitted by three implications; checks all raw worker pairs remain within the six legal tuples, all three contexts are screened, new independent matched seeds are used, SQLite design replay is deterministic under pause/resume, pre/post-commit forced exits recover, edited decision/seed/raw evidence is rejected, malformed rules fail closed, and v12 still rejects v13 input.

**Status:** code authored, exact-head CI required. V3-04 **PARTIAL** because full multi-axis region splits, cost-aware acquisition, complete accounting, frozen held-out comparison and statistical confidence remain open.

## V3-04 conditional region frontier successor (schema v14)

New `ConditionalPairPortfolio(..., region_tree=true)` adds bounded per-context legal-edge intervals, deterministic midpoint bisection, issued-edge uniqueness, compiled-rule and rare-stratum preservation, and distinct region-state digests. This is opt-in `phaseshift_v3_pairs conditional-tree` creating schema v14, with v10–v13 replay unaffected. The executor and raw-worker schema are unchanged apart from the additive explicit new protocol version.

Tests: `tests/v3/region_tree.cpp` independently enumerates 62 edges of a legal two-context 32-point domain with two injected opposing cliff locations, tests deterministic replay, null/nonfinite/holdout feedback and 2.048B virtual-space cap. `tests/v3/region_tree_worker_cli.py` tests real workers, both crash boundaries, rare strata, tampered choice/seed/effect and both disjoint and implication legality.

**State:** code authored; exact-head GCC/Clang/ASan/coverage still to verify. Master V3-04 stays **PARTIAL**: no development-suite net discovery performance superiority, no calibrated CPU/parent overhead envelope and no universal categorical interaction or inference claim.

## V3-05 auditable empirical acquisition (schema v15)

Recovered exact prior checkpoint `a7662acf1f4ecacde52cb0b5c2cc1050aadcec3c`: all four exact-head GitHub checks succeeded (GCC, Clang, ASan/UBSan, coverage). This historical verification is not a new-head result.

`acquisition-v3` is a separate opt-in v15 protocol. `acquisition.hpp` implements bounded committed-only cost estimation with lower medians, cold-category pooling, empirical maximum envelope and separate wall/worker-region CPU predictions. Every fourth committed pair is reserved for prequential evaluation and excluded from training. Ranking costs turn on only after six fit pairs, two heldout pairs, observed >=80% wall-envelope coverage and no censored pairs. Otherwise the cost penalty is neutral. This is an engineering eligibility rule, **not** statistically calibrated prediction coverage. Failed attempts are recorded separately; they never acquire finite speed. Scores are clipped, dimensionless, explicitly decomposed, with component-level no-cost/no-uncertainty/no-replicate/no-diversity/no-interaction ablations. The default experiment retains forced rare/global exploration and fixed replication opportunities; cost can never veto these. The v15 region and replication candidate rank uses observed contrast, uncertainty, novelty, rival-sign diagnostic and status-frontier signals. Rival signs are not a validated interaction estimate.

SQLite stores exact score bits, cost model diagnostics and actual wall/CPU alongside atomic paired worker commits. Replay rebuilds cost history, independently totals raw worker CPU and rejects mismatched diagnostic/last-pair cost. Legacy v10–v14 policy behavior and schema identities preserved. `tests/v3/acquisition.cpp` attacks cold start, drift, censored/malformed data, cost-conditioned ranking, ablation math and deterministic feedback replay; `tests/v3/acquisition_worker_cli.py` independently checks 12 real matched pairs, the frozen holdout split, score calculation, cost ledger, forced failures and tampering.

**Status:** V3-05 PARTIAL, code/tests authored; exact-head CI pending. Full elapsed parent/process-tree CPU and abandoned costs, externally frozen development distributions, CPU interval evaluation, production CLI ablation campaigns, calibration coverage uncertainty and multi-family net gain remain OPEN. No policy promotion or SOTA claim.
