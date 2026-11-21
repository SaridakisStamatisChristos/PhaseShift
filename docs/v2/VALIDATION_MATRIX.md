> **Latest verified code checkpoint:** `60be88bd`: GCC 73/73 CTests, Clang 73/73, ASan/UBSan 73/73, coverage 73/73 and 95.11% production executable line coverage (4902/5154), all GitHub-hosted workflows green on this **code SHA**. Current docs revision is not automatically covered. H2+ nested witness bundles and matched-contrast heuristic replication are now implemented and tested, but statistical calibration, genuine equal end-to-end CPU/wall budgets, and external SOTA comparison remain open. See [CONTINUATION_STATUS.md](CONTINUATION_STATUS.md).

> **Status notice (R7 continuation, 21 November 2026):** This file contains historical checkpoint observations; consult [CONTINUATION_STATUS.md](CONTINUATION_STATUS.md) for the current exact-revision evidence and open hard gates. Earlier “pending latest CI” statements describe older revisions, not the current branch head.

# PHASESHIFT v2 — Validation Matrix (provisional; not a release gate pass)

| Gate | State | Evidence / remaining work |
|---|---|---|
| G0 baseline revision | VERIFIED | main `93b47590c7e44eaf062d97995e9c5cd33274a7a2`; original 3 workflows green |
| G1 legacy v1 | LOCALLY/CI VERIFIED ON INTERMEDIATE SHA | Original v1 CTest and CLI through GitHub CI, final exact SHA pending |
| G2 lazy huge space | TESTED INTERMEDIATE | >10^9 virtual cardinality without enumeration; max-memory/memory metrics not fully published |
| G3 deterministic resume | TESTED INTERMEDIATE | v2 uniform/hierarchical pause/resume CLI and scheduler transcript; crash injection pending |
| G4 search correctness | PARTIAL | unit tests, quotas, seed replay; conditional feasible-space and adaptive pairing pending |
| G5 multi changes | PARTIAL | exact DP brute-force oracle and opt-in worker fixture; noisy null/sealed trials pending |
| G6 conditional interaction | PARTIAL | 2x2 matched cohort, status separation, real worker event; higher-order and full isolation pending |
| G7 campaign holdout | PARTIAL VERIFIED | M7 fixed-family two-hypothesis real-worker test and CLI/SQLite Holm finalization passed 58/58 at c2cb1fb; no broad calibrated familywise error claim |
| G8 measurement protocols | PARTIAL / PENDING LATEST SHA | Legacy v1 retained; opt-in calibrated_batch_v2 separated pilot/measurement, strict mode validation, worker unit and real resume test added; independent latest CI pending |
| G9 minimization | PARTIAL | Legacy oracle-preserving minimizers retained; conditional/multi-boundary v2 witness and independent seed provenance still incomplete |
| G10 CI | PENDING FINAL SHA | Track PR #4 exact SHA |
| G11 95% line coverage | VERIFIED INTERMEDIATE ONLY | `c2cb1fb`: 58/58 CTests, line 95.62%, GCC/Clang and ASan/UBSan green; latest M6/M9 revisions pending |
| G12 benchmark fairness | PARTIAL | actual worker/CPU/wall scorecard, equal case budget; equal CPU+wall budget missing |
| G13 research uplift | UNVERIFIED | no held-out uplift claim |
| G14 external SOTA | UNVERIFIED | no executed matched external tools |

States are deliberately strict; no intermediate green workflow grants final-branch or SOTA approval.

## R7 continuation gates (draft branch)

- M2 exact indexed admissible-union sampler: **CI_VERIFIED at `1fc3e77`**, with 48/48 CTests, GCC/Clang, ASan/UBSan and 95.93% production line coverage.
- M3 `stratified_v2`: implemented, per-primary-stratum replay + real SQLite smoke, latest CI to verify.
- M5 `multi_change_pelt_v2`: pruned exact squared-error detector with hard K fallback, exhaustive independent DP differential test + real worker ground truth smoke. Not called CI_VERIFIED until latest revision passes.
- M7/M9/M10: predeclared campaign family, additive schema migration and fair equal-CPU/wall + external baselines remain UNVERIFIED. No family-wise error or SOTA certification.

- M1 opt-in measured timed-region CPU stopping limit: per-case ledger, crash/resume raw-cost derivation, v1 isolation and invalid-cost refusal; only partial full experiment-budget contract. Full CPU/wall accounting and equal-budget SOTA still outstanding.

- M9 SQLite additive v8 opt-in: newly implemented, allows historical v7 read-only and v1 untouched; budget ledger FK and atomic transaction tests added. Does not yet implement full campaign, model-cache or candidate-key table set. Independent latest-revision CI pending.

- M5/M7 ranked multiple candidates: up to 16 distinct geometric, HYPOTHESIZED-only H1..H16 with explicit frozen candidate data; unique v2 holdout seed streams. Provisional until a real multi-change/real-worker campaign passes and family correction is sealed.

- M6 separate calibrated worker pilot: pilot correctness, final timed output validation and noninterference with legacy timing implemented. End-to-end CLI and per-run version selector remain the next verification step; no precision gain established.

## Exact revision gates — continuation

Latest source changes include calibrated timing, seed-isolated campaign seals, dependency-aware null stress and budget-ledger cross-validation. These are **PENDING exact-SHA CI**; do not infer passing from the prior green 58/58 run. The separate synthetic null test does not constitute empirical engine-wide familywise error calibration. No equal full CPU / wall budget comparison or external matched competitor has been performed.

- M9 crash/retry matrix: direct simulated failure injection after observation, after scheduler decision and after CPU-budget ledger write, each before COMMIT, checks full SQLite rollback and successful exact restart of the same canonical case. Real OS kill/power loss and all future schema tables remain additional gates.


## 21 November second R7 batch

`weighted_v2` sparse weighted without-replacement proposals, bounded labelled priors and admissible rectangles plus transactionally stored per-case wall and timed CPU evidence are implemented on code SHA `1db4133`. Tests cover direct billion-point virtual space, weighted-selection skew, exhaustion, strict invalid schema, pause/resume replay and independent SQLite event linkage.

**Acceptance state:** M1 PARTIAL, M2 PARTIAL, G3/G10/G11 PENDING exact-SHA full CI; only existing v1 and earlier v2 code are verified at previous `aacddb0` checkpoint. G12 fair full-system CPU and wall, G13 uplift and G14 external comparison remain UNVERIFIED. Preserve both positive and negative test outcomes and do not claim SOTA.
