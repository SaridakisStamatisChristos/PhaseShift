> **Current-state notice (22 November 2026):** PR #5 has been merged; v2 baseline `main` is `279a8c65ad7ec3741a727ff49aa7f84e0628a6f1` at this checkpoint. The original exact-main jobs passed 93/93 CTests per lane and 95.19% production line coverage. See [v3 baseline provenance](../v3/BASELINE.json) and [historical research scorecards](../v3/V2_HISTORICAL_SCORECARDS.json). Statements below are original historical checkpoints, not live PR/branch status. Expensive research was observed on its earlier PR merge checkout, not established on exact merged main.

> **Current status (21 November 2026, PR #5):** PhaseShift v2 was merged into `main` as `8e795197b65af9ac73abff84bfaaefe29d6c92fb` (PR #4). New R7 completion work is in **draft PR #5**, not merged. The status and checklist in [R7_COMPLETION_CAMPAIGN.md](R7_COMPLETION_CAMPAIGN.md) supersede older draft/old-main statements below, which are preserved solely as historical evidence. The experimental code needs exact-SHA CI and held-out performance verification; no SOTA claim.\n\n> **Latest verified code checkpoint:** `60be88bd`, with GitHub GCC/Clang/sanitizer 73/73 CTests each and LLVM production line coverage 95.11% (4902/5154). See [CONTINUATION_STATUS.md](CONTINUATION_STATUS.md#r7-continuation-checkpoint--21-november-2026-verified-code-revision-60be88bd) for exact workflow IDs, new commits and M0–M12 remaining hard gates. This documentation-only commit requires its own CI; older checkpoint records below remain historic.

> **Status notice (R7 continuation, 21 November 2026):** This file contains historical checkpoint observations; consult [CONTINUATION_STATUS.md](CONTINUATION_STATUS.md) for the current exact-revision evidence and open hard gates. Earlier “pending latest CI” statements describe older revisions, not the current branch head.

# PHASESHIFT v2 — R7 implementation ledger

Baseline main: `93b47590c7e44eaf062d97995e9c5cd33274a7a2` on 2026-11-21. Existing push CI `build-test`, `sanitizers` and `production-line-coverage` succeeded on **this original main SHA only**.

## Hard invariants

Preserve historical generator bytes/IDs, v1 CLI behavior, SQLite v7 evidence, native plugin ABI, independent correctness oracle, sealed holdout, worker containment, and >=95% LLVM production line coverage. Use explicit v2 opt-in. Do not claim new inference or SOTA without executed equal-budget evaluation.

## R7 requirement / claim ledger

| Requirement | State | Evidence or gap |
|---|---|---|
| G0 main pinned | VERIFIED | SHA above and successful original push CI |
| M0 frozen SOTA benchmark contract | PARTIAL | Frozen scenario inventory and real-worker four-policy cliff/null diagnostic completed; equal worker-CPU/wall and external baselines outstanding |
| M2 lazy mixed-radix index and uniform sparse sampler | PARTIAL | Uniform, exact disjoint admissible subspaces and stratified lazy union implemented, tested at 1fc3e77 / 5aa334e; weighted priors, structural constraints beyond rectangles and large-scale RSS gate remain |
| M1 explicit policy version | PARTIAL | v1 default, v2 opt-in; persisted CPU-budget ledger plus raw replay audit implemented; incremental evidence-index cache, parent CPU/wall accounting remain |
| M5 multi-change model | PARTIAL | Exact K-bounded DP, PELT with correct fallback, exhaustive differential oracle and ranked H1..H16 candidates; independent family testing available; slope models and noisy held-out localization remain |
| M3 hierarchical boundary policy | PARTIAL | Opt-in hierarchical, stratified and uniform lazy search with replay; no calibrated uncertainty or fully adaptive mixed-condition acquisition / held-out superiority |
| M7 multi-hypothesis correction | PARTIAL | SQLite v9 frozen family, positive independent holdout seal+p-value, CLI finalize, H1..H16 distinct seeds and real-worker two-H1 test green at c2cb1fb; population selection leakage / null calibration still incomplete |
| M10 frozen calibration comparison harness | PARTIAL | Four-policy cliff/null real-worker challenge, frozen plan and raw worker CPU/wall; equal-case ceilings only, not external/matched CPU-wall SOTA |
| M4 controlled two-way interaction | PARTIAL | Independent 2x2 matched-seed difference-in-differences and status contrast; explicit invalid/partial abstention; opt-in v2 emits an immutable discovery event only, not independent confirmation or higher-order interactions |
| M6, M8–M9, M11–M12 | PARTIAL | M6 pilot-separated timing and opt-in protocol added (latest CI pending); M9 v8/v9 transactional persistence + budget replay; M8 advanced conditional witness, M10 external evaluation and remaining acceptance gates outstanding |
| G1–G14 v2 hard gates | UNVERIFIED | No branch CI or held-out comparison yet |

Research claim: **none**. Any synthetic fixture is a correctness calibration, not scientific novelty. No PR merge, license or release is authorized.

## CI falsification / repair F-v2-001

Commit `1e3cf349` ran 39/39 CTests successfully in the LLVM coverage job, but the coverage gate **failed**: `src/boundary_v2.cpp` was missing from the instrumented linked object set. Earliest affected phase: INTEGRATE (coverage aggregation), not the detector algorithm. Repair: explicitly include the executable that links the new translation unit in `llvm-cov export`; rerun coverage on the repaired branch SHA. This is not an accepted pass until exact-revision CI reports success.

## Workflow hardening

The built-in equal-case-budget comparison is an **ephemeral CTest scratch benchmark**. The wrapper cleans only its own CMake build-directory fixture before rerun. Real benchmark evidence created by the direct `equal_budget.py --out ...` invocation is immutable and the tool refuses to overwrite it. `multi_change_v2` retains any v1 correctness/resource/crossover boundary in preference to a weaker timing-only segmentation.

## Immutable R7 milestone commit trail (feature branch, draft-only)

Main/source baseline: `93b47590c7e44eaf062d97995e9c5cd33274a7a2`; target PR: https://github.com/SaridakisStamatisChristos/PhaseShift/pull/4 (draft; NOT MERGED).

| SHA prefix | Coherent change | State |
|---|---|---|
| `561300e` | Freeze scenario inventory, huge lazy search sampler and unit tests | Reviewed/source-pushed |
| `0dec800` | Opt-in v2 uniform runner and strict replay | Coverage and sanitizer CI green at intermediate SHA |
| `1e3cf3` | Bounded exact multi-change DP and differential oracle | 39/39 CTests in coverage job; LLVM object missing (not passing gate) |
| `764b18` | Cost/contrast hierarchical policy and CLI | Initial CI revealed missing forward declaration |
| `653a20` | Fix accessor declaration | Intermediate verification, not a release |
| `2318928` | Fix LLVM export object linkage for DP | GitHub 41/41 CTests, 96.08% production line, 71.02% branch coverage |
| `d007996` | First Holm primitive implementation | Initial reference expectation incorrect, superseded |
| `a1ceb0` | Correct Holm independent reference test | GitHub 41/41 CTests, 96.10% production line, 71.13% branch coverage |
| `c1d55a` | Frozen real-worker v1/v2 single-cliff comparison harness | Exact SHA CI pending; actual benchmark scorecard still unverified |
| `6d31dd` | Optional independent per-coordinate seed replicates and resume evidence checks | Exact SHA CI pending |
| `bf1944` | 2x2 matched-seed performance/status interaction with CLI discovery event | Exact SHA CI pending |
| `f4cd00` | Opt-in multiple-change detector runtime selection and event evidence | Exact SHA CI pending |
| `df3294` | Publication of protocol/validation/SOTA-status docs and repeatable test scratch | Exact SHA CI pending |

### Evidence-granularity safeguards
- The `2318928` coverage workflow (<https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44830727867>) is **only** evidence on that revision, not on subsequent commits.
- The `a1ceb0` coverage workflow (<https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44831065345>) is **only** evidence on that revision, not on subsequent commits.
- Full GCC/Clang build matrix, sanitizer, coverage and CTest status on **the latest branch revision** remains PENDING at this ledger edition. The full draft PR is not eligible to merge until those hard gates pass.
- There is **no** executed external benchmark or statistically defensible full SOTA uplift claim.
- Outstanding milestone-critical features remain M6 measurement protocol, M7 persistent campaign family/holdout control, M8 v2 witness minimization, M9 additive crash-safe migration, M10 external/equal actual-budget evaluation, and adversarial/end-to-end M11 gates.

## R7 continuation (21 Nov 2026): algorithmic improvements and repairs

| Revision | Implementation | Verification boundary |
|---|---|---|
| `0887ffec` | Assert side effects repaired for Release | 46/46 CTests, GCC/Clang, sanitizers, 95.97% production line coverage |
| `1fc3e77` | M2 indexed non-overlapping admissible Cartesian subspaces | **48/48 CTests; 95.93% production line / 70.84% descriptive branch**, coverage run 44873253216; GCC/Clang and sanitizers successful |
| `433738d` | M3 lazy seeded primary-axis stratification | Superseded by independent SQL query repair |
| `5aa334e` | Stratified SQLite replay audit corrected to order by actual seq | Exact revision workflows required |
| `04ae1ea` | M5 PELT with constrained exact-DP fallback | GCC compiler failure: declaration outside namespace; unreleased |
| `d2569d4` | Move PELT declaration inside namespace ps | New exact-revision CI required |
| New PELT smoke commit | Real worker/Python diagnostic test and coverage object; this ledger | Verification pending |

**F-v2-002:** GCC Release failed on `04ae1ea` with `MultiChangeFitV2 does not name a type` in `boundary_v2.hpp`. Cause: accidentally appended PELT declaration after the namespace closed. Repaired with `d2569d4`; CI exact-revision revalidation is required. Earliest invalidated phase: INTEGRATE (header/API scope).

**Remaining hard requirements:** M1 indexed evidence cache / actual budget ledger; M3 calibrated cost/uncertainty hybrid; M4 conditional independent confirmation; M6 distinct verified measurement protocols; M7 sealed predeclared campaign family Holm control; M8 v2 causal witness minimization; M9 additive v7→v8 migration; M10 genuine equal worker CPU+wall budgets and held-out external SOTA comparison. V1 default and historical evidence remain invariant; no SOTA or release claim until executed.

### M9 opt-in additive SQLite v8 persistence slice
The v2 workflow now calls `Store::upgrade_v2_schema()` explicitly before writing its experiment, leaving all v1-created v7 databases unchanged. Schema v8 adds a policy-version register and foreign-keyed, exactly-once case/decision sampled-CPU ledger committed in the same transaction as raw cases and the scheduler decision; a SQL ledger missing/mislinked row fails closed. An independent schema migration/read-only test validates v1 schema isolation, v7→v8 preservation and v9 rejection. All other proposed M9 tables and complete crash-injection tests remain outstanding. **CI gate pending on this revision.**


## M7 registry slice — frozen family/SQLite v9 (21 Nov 2026)
Predeclared, one-family-per-v2-run immutable campaign manifest and Holm outcome table implemented with explicit positive seal+p-value requirements, conservative p=1 for negative members, re-freeze/re-finalize refusal, and independent schema-v9 migration tests. Execution from CLI and complete multi-hypothesis lifecycle remain separate verification gates. No global FWER/SOTA claim.

### M9 replay-ledger falsification and repair
V2 sampled-CPU replay now checks each persisted scheduler decision's foreign-keyed `budget_ledger` cumulative CPU, declared limit, and accounting-completeness flag against values independently reconstructed from raw measurements. A fully finalized budget-limited run still executes this read-only proof path on `resume`, rather than short-circuiting straight to report generation. The CLI test now uses a transactionally backed-up corrupted copy with the cumulative CPU incremented by 1 ns and requires fail-closed rejection; noncorrupted repeated resume remains idempotent. Legacy v1 unaffected. Exact SHA CI required.

### M7 multi-candidate fail-closed gate
`confirm` refuses unfrozen v2 H2..H16 and multi-candidate H1, with real-worker adversarial CLI test for zero holdout before freeze; it then permits the predeclared Holm family. Legacy v1 and single-candidate v2 unchanged. Latest exact revision CI required.


## R7 subsequent implementation batch — weighted v2, per-case costs

- `96d24ca`: sparse product-weighted admissible-domain sampling, seed replicates, canonical virtual indices.
- `f189dfd`, `903c15f`: explicit search priors in Config and strict v2-only read/write, validation, replay and experiment execution.
- `18797dd`, `c4ae802`, `d69033b`, `52e2097`, `d5481a9`: unit/real-worker 1e9 virtual space, finite prior validation, pause/resume independent SQLite oracle.
- `9bcd6f1`: repair compiler error in test lambda default argument (F-v2-010).
- `397d12b`: transactionally persisted phase-wall and timed sample CPU descriptive accounting, read-only audit of event-to-decision mapping.
- `e6bee64`: 17 invalid priors and v1 policy boundary checks.
- `1db4133`: repair replay-oracle SQL query to use persisted `rng_state` (F-v2-011).
- Exact acceptance of this new batch: **PENDING 1db4133 CI**; a previous 75-test coverage workflow demonstrated 74 successes plus an invalid test query, not feature acceptance. Complete line/branch coverage must be remeasured. No code is promoted to v1 default, and no work is merged.

R7 requirement-to-evidence closure: M2 weighted policy = IMPLEMENTED/PARTIAL VERIFIED; M1 phase-cost logging = IMPLEMENTED/UNVERIFIED on latest; G1 compatibility, G3 replay, G10 CI and G11 coverage await exact-sha checks; G12–G14 remain not established.
