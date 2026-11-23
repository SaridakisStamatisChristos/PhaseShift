> **Current-state notice (22 November 2026):** PR #5 has been merged; v2 baseline `main` is `279a8c65ad7ec3741a727ff49aa7f84e0628a6f1` at this checkpoint. The original exact-main jobs passed 93/93 CTests per lane and 95.19% production line coverage. See [v3 baseline provenance](../v3/BASELINE.json) and [historical research scorecards](../v3/V2_HISTORICAL_SCORECARDS.json). Statements below are original historical checkpoints, not live PR/branch status. Expensive research was observed on its earlier PR merge checkout, not established on exact merged main.

# PhaseShift v2 — UCS-Pro R7 completion campaign

**Base:** merged `main` commit `8e795197b65af9ac73abff84bfaaefe29d6c92fb` (PR #4). **Working PR:** [#5](https://github.com/SaridakisStamatisChristos/PhaseShift/pull/5), `feature/phaseshift-v2-r7-completion`, DRAFT and NOT merged. **Experimental source checkpoint:** `64f34adc3cea2d68c6c4f0e14403b122f0533e6d`, exact-workflow verification still required at this document writing. This document supersedes historical references to an unmerged PR #4, but retains their actual earlier CI provenance.

## R7 requirement contract

The source-of-truth implementation directive is `PHASESHIFT_V2_SOTA_UPGRADE_MASTER_PLAN.md`. Core invariants: unchanged v1 config/default/generator/case IDs/ABI/SQLite v7, explicit v2 opt-in, correct independent oracles, sealed holdouts, exactly replayed decisions, <= stated resource ceilings and no fabricated empirical/SOTA claims. Every partial result below identifies its acceptance gap.

| Plan | Status | What is implemented and independently testable | What is still not validated/implemented |
|---|---|---|---|
| M0 frozen benchmark baseline | PARTIAL | Prior original v1 main pinned, fixed cliff/null policies and new immutable full-CLI two-ceiling plan | Broad frozen ground-truth benchmark and complete external matched search baseline |
| M1 evidence/cost engine | PARTIAL | Bounded committed-only indexed evidence view; once-per-replayed-decision SQL read; parent+waited child CPU, proposal/worker wall in same source transaction; full-CLI CPU/wall ceilings & upper-bound residual | True complete child process-tree accounting under every OS failure, calibrated acquisition cost |
| M2 high-dimensional feasible sampling | PARTIAL | Exact bounded conjunction of typed conditional implications, compiled into disjoint indexed rectangles; weighted sparse sampling without replacement, large virtual-universe test and measured peak RSS test | Arbitrary symbolic feasible constraint grammar beyond 32 bounded rules / 256 rectangles; broad generator-coupling proof |
| M3 cost-aware acquisition | PARTIAL | Hierarchical interval candidate heap replaces repeated full scan, original oracle sequence retained; hybrid matched fresh-seed replication and budget measurement | Validated posterior/uncertainty calibration and held-out superiority |
| M4 conditional causal contrasts | PARTIAL | Complete discovery 2x2, fixed independent 20-seed real-worker factorial holdout, explicit seed disjointness and one-sided sign p; independent results frozen in separate SQLite; no false global confirmation | Link all valid interaction claims into frozen campaign-wide Holm family & independent structural intervention evidence |
| M5 changepoint taxonomy | PARTIAL | Exact DP, PELT constrained fallback, piecewise slope; planted null/cliff/smooth/outlier 20-seed independent fixture, preserves all losses | Broad noisy localization confidence coverage and calibrated family-wide discovery errors |
| M6 measurement | PARTIAL | Calibrated-pilot opt-in, raw drift diagnostics and additive log-space median absolute deviations | Repeated-device precision/variance gains and drift p-values are NOT demonstrated |
| M7 selective inference | PARTIAL | Frozen independent multi-hypothesis Holm campaigns, correlated 3,000-null campaigns (unit level) | Large real-engine correlated-null FWER and selection leakage study |
| M8 counterexample minimization | PARTIAL | H1/H2 and schema-2 portable evidence, strict 2-sided admissible conditional scope guard on candidate minimizations | All conditional/multi-boundary independent fresh-seed minimization coverage |
| M9 persistence | PARTIAL | Immutable SQLite v8/v9 source, SIGKILL before COMMIT tests, opt-in **reconstructable sidecar** SQLite cache, independent `verify-model-cache` read-only replay, missing checkpoint repair and mismatch/ahead rejection | Full persisted model state as an authoritative resumable model and exhaustive machine-crash matrix |
| M10 fair competitive benchmarks | PARTIAL | Case-level measured CPU/wall, both-ceiling stopping with overshoot explicit, synthetic and actual sorting/Top-K/graph workers, paired audit scorecards, detector-only external ruptures PELT adapter for truly common L2 objective | 20 independent seed raw campaign results, matched external *search* competitors like SMAC3/PerfFuzz, full statistical superiority |
| M11 CI quality | VERIFIED ON PREVIOUS MAIN, CURRENT PR PENDING | GCC/Clang, ASan/UBSan, >=95% production line gate; new tests compile/execute under same workflows | Current exact head all green remains mandatory before merge |
| M12 publication/readiness | PARTIAL | Exact-revision R7 ledger, reproducible source, immutable scorecard formats, no silent exclusions | Evidence-complete public SOTA verdict (positive or negative) and release acceptance |

## Implemented source changes

- `conditional_space_v2.hpp`, strict JSON config parser, actual v2 worker integration, exhaustive small-domain oracle + 1e9 virtual-domain test and independent pause/resume SQL verification.
- `evidence_view_v2.hpp` committed-only cache, `scheduler_v2.hpp` lazy-invalidated priority queue with the original full-scan oracle, `cpu_usage_v2.hpp` Linux parent+waited-worker accounting.
- `model_cache_v2.hpp`: independent SQLite **derived** cache. Source observations and `run.sqlite` decisions are authoritative; partial cache records are rebuilt by `verify-model-cache`, discrepancies fail closed; finalized `resume` behavior is deliberately unchanged.
- `confirm-interaction --run`: predeclared 20×4 independent factorial worker validation, immutable seed/manifest/output, no global confirmation without campaign seal.
- `tests/v2_noisy_detection.cpp` (20 seeds per 6 scenarios), `tests/v2_large_space_rss.cpp` (observed Linux RSS rather than assumed memory usage), `measurement_drift_v2.hpp` robust log-space MAD descriptions.
- `benchmarks/v2/dual_cap_challenge.py`, `real_family_dual_cap.py`, `summarize_dual_cap.py` and `external_ruptures.py` produce frozen raw, recorded-loss, complete seed-policy inventory; no SOTA output flag can be true.

## Adversarial failure/repair ledger

- **R7-F01** Original invalid-config fixture called a logically satisfiable implication set impossible. Corrected to genuinely contradictory rules and reran all CTests.
- **R7-F02** New fixed-factorial unit fixture failed to include/qualify the repository `ps::Error` type; fixed rather than weakening tests.
- **R7-F03** Initial model-cache implementation had misleading C++ indentation under strict `-Werror`; repaired in source.
- **R7-F04** Real-workload benchmark input lacked an explicit `limits` object, previously assumed present by synthetic fixtures; added `setdefault` to preserve both styles.
- **R7-F05** Model-cache auditor initially attempted `resume` after hypothesis finalization, which v1/v2 must refuse. Added `verify-model-cache` to perform read-only source replay and derived sidecar repair with no reanalysis or status rewrite.
- **R7-F06** Model-cache integer CPU tally previously included invalid samples if the worker supplied a cost even though `Store::add_observation` canonicalizes invalid CPU as NULL; made cached sums identical to committed source SQL.

## Execution / exact gates

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure

# Diagnostic only: same CPU and wall ceilings across policies, not equal consumption
python3 benchmarks/v2/dual_cap_challenge.py \
  --binary build/phaseshift --out /tmp/phaseshift-20-synthetic \
  --seeds "$(seq -s, 1001 1020)" --cases 8 \
  --cpu-budget-ns 5000000000 --wall-budget-ns 15000000000
python3 tests/verify_v2_dual_cap.py --root /tmp/phaseshift-20-synthetic
python3 benchmarks/v2/summarize_dual_cap.py \
  --root /tmp/phaseshift-20-synthetic \
  --output /tmp/phaseshift-20-synthetic/scorecard.json

python3 benchmarks/v2/real_family_dual_cap.py \
  --binary build/phaseshift --out /tmp/phaseshift-20-real \
  --seeds "$(seq -s, 1001 1020)" --cases 8 \
  --cpu-budget-ns 5000000000 --wall-budget-ns 15000000000
```

See `benchmarks/v2/EXTERNAL_PELT.md` for the separately pinned offline detector-only ruptures comparison. Synthetic calibration is not automatically a true performance boundary in arbitrary real workloads. A 20-seed experiment, by itself, does not imply SOTA or correct family-wise inference. Preserve all losses, failed commands and case-boundary budget overshoots as observations.

**Release/merge:** PR #5 is draft; do not merge, retitle code as production-ready, release, or make scientific novelty claims without verified gates and user authorization.
