# PhaseShift v3 claim audit (V3-11)

**Date:** 2026-11-23. **Branch:** developed on `feature/phaseshift-v3-sota-research` (PR #6), **merged into `main` as `e7169dd`** with explicit owner approval on 2026-11-23. **Status:** the evidence verdict is FINAL. The one-time external confirmation was executed and both hypotheses were not supported. The path forward is planned in [`SOTA_ROADMAP.md`](SOTA_ROADMAP.md); the next session starts from [`../HANDOFF.md`](../HANDOFF.md).

This audit states, per claim, what the evidence supports. Source, tests, Git history and recorded CI runs are the only evidence; a document describing a feature is not. "Verified" means observed execution on the exact commit named.

## Verdict summary

| Claim type | Status | Basis |
|---|---|---|
| v3 engineering correctness (builds, tests, sanitizers, coverage) | **VERIFIED at exact SHAs** | All CI lanes green at `4da55d7`: GCC and Clang 122/122, ASan/UBSan 122/122, production line coverage 7384/7724 = 95.60% (gate 95%) |
| v1/v2 behaviour preserved | **VERIFIED (sampled)** | `v3-historical-compatibility` green at every head from `158011e` to `4da55d7`: frozen schema 7/8/9 fixtures, replay and report identity |
| Evidence integrity (journaled holdout, signed and minimized witnesses, fault containment, storage faults) | **VERIFIED engineering, with stated limits** | V3-09b–f, F-027–F-031; each mechanism has falsifying tests, several mutation-checked |
| Internal v3 policy superiority | **NOT ESTABLISHED** | One-time sealed V18 evaluation (opened, 360 trials): 53/52/50 positive hits, point estimates only, `superiority_established: false` |
| External advantage over SMAC3 2.4.1 | **NOT SUPPORTED (confirmed negative)** | One-time frozen confirmation, run 45211496039: 5 better / 4 worse / 11 tied clusters, Holm p = 1.0. The development signal (+0.4) did not replicate |
| External advantage over Optuna 4.6.0 TPE | **NOT SUPPORTED (confirmed negative)** | Same run: 5 better / 7 worse / 8 tied, mean −0.10, Holm p = 1.0 |
| External competitiveness in general | **UNPROVEN** | No external advantage is established on any tested endpoint |
| Change-point detector parity with ruptures PELT | **VERIFIED, detector-only scope** | Identical penalised objective on all 100 matched series (abs diff ≤ 2.2e-16); not an acquisition or end-to-end claim |
| State of the art | **NOT CLAIMED, and not supported** | The only confirmatory external test found no advantage |

## External comparison evidence (V3-10)

All executed results are archived verbatim with SHA-256 in [`external_evidence/`](external_evidence/MANIFEST.json).

| Comparison | Cohort | Executed run | Outcome |
|---|---|---|---|
| V18 vs Optuna 4.6.0 TPE and random, real sort/Top-K, 120 campaigns | development 32001–32020 | 45142307852 @ `8cef6a7` | No difference. V18 − TPE mean 0.0 (exploratory 95% [−0.45, 0.45]); V18 − random −0.1 [−0.6, 0.35] nominal rejections per seed cluster |
| V18 vs SMAC3 2.4.1, 80 campaigns | development 33001–33020 | 45143027905 @ `848e2b7` | V18 − SMAC +0.4 (exploratory [0.1, 0.7]; 8 positive, 1 negative, 11 tied clusters). V18 discovery CPU ≈ 12.7–13.3 s vs SMAC ≈ 35.2 s per scenario; optimizer and process overhead **not matched** |
| C++ exact DP vs ruptures 1.1.10 PELT | 100 detector series | 45117904998 @ `b3fde1a` | Objective-identical on matched feasible sets |

These are **hypothesis-generating** results. They were not frozen in advance, use no labelled ground truth, and their percentile intervals are exploratory.

### One-time confirmation result (executed 2026-11-23)

[Run 45211496039](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/45211496039) ran on harness commit `4525c50` and was opened by `workflow_dispatch` after the guard confirmed no previous opening.

- **Completeness:** status `COMPLETE`, 40/40 trials, no failed or missing trials.
- **Binary identity:** native binary built from `848e2b7`, SHA-256 `a60e1adc…`, byte-identical to the development SMAC study's binary.
- **Archive:** the audited scorecard is [`external_evidence/external_confirmation_one_time_final.json`](external_evidence/external_confirmation_one_time_final.json) (SHA-256 `80e996fb…`). It was independently recomputed byte-identical from the raw trial artifacts on a second machine. Raw artifact digests are recorded in the evidence manifest.

| Hypothesis | V18 better | V18 worse | Ties | Mean diff | raw p | Holm p | Decision |
|---|---|---|---|---|---|---|---|
| H1 V18 > SMAC3 2.4.1 | 5 | 4 | 11 | +0.05 | 0.50 | 1.0 | NOT SUPPORTED |
| H2 V18 > Optuna TPE | 5 | 7 | 8 | −0.10 | 0.81 | 1.0 | NOT SUPPORTED |

Descriptive only (not cost-matched; process and optimizer overhead differ):

| Scenario | Policy | Rejections / nominations | Discovery wall (s) | Child CPU (s) | Harness CPU (s) |
|---|---|---|---|---|---|
| sort_ascending | V18 | 40 / 40 | 14.3 | 12.0 | 0.07 |
| sort_ascending | SMAC3 | 40 / 40 | 62.2 | 13.8 | 46.5 |
| sort_ascending | Optuna TPE | 39 / 40 | 17.1 | 13.6 | 0.84 |
| topk_uniform | V18 | 33 / 40 | 17.0 | 14.8 | 0.41 |
| topk_uniform | SMAC3 | 32 / 37 | 71.6 | 17.0 | 53.3 |
| topk_uniform | Optuna TPE | 36 / 40 | 20.3 | 17.0 | 0.92 |

Per the frozen decision rule: **no advantage over either competitor is established, and external competitiveness on this endpoint remains unproven.** On these two workloads all three policies nominate fresh-seed-reproducible contrasts at similar rates. The endpoint saturates on `sort_ascending`, where all policies reach 39–40 of 40 nominations, which limits its power to separate them.

### Frozen protocol (as executed)

[`benchmarks/v3/external_confirmation.json`](../../benchmarks/v3/external_confirmation.json) fixes the following before any cohort observation:

- **Binary:** built from development commit `848e2b7`, whose three V18 source blobs produced the hypotheses.
- **Optimizers:** SMAC3 2.4.1 and Optuna 4.6.0 TPE adapters, imported unchanged from the development runners. One V18 run per seed is shared by both comparisons.
- **Cohort:** 51001–51020. Every derived discovery, query and confirmation seed is mechanically shown disjoint from every seed list in the repository and from all prior derived seeds.
- **Endpoint:** fresh-seed nominal rejections per seed cluster.
- **Hypotheses:** H1 V18 > SMAC and H2 V18 > TPE, each a one-sided exact sign test, with Holm correction across both at 0.05.
- **Failure rule:** any missing or failed trial means `INCOMPLETE_NO_CLAIM`, and the cohort is never rerun.

The workflow opens the cohort only on an explicit `workflow_dispatch` token, and refuses any second opening.

**Allowed claim after execution:**

- If H*i* is supported: "V18 nominates more fresh-seed-reproducible matched contrasts than *competitor* on these two real workloads, this host class and a fixed 12-query budget."
- Otherwise: "external competitiveness on this endpoint is unproven."

Neither outcome is a SOTA claim. Real boundaries are unlabelled, sign independence is assumed rather than certified, and costs are not matched.

## Milestone status

These states come from `IMPLEMENTATION_LEDGER.md`. Milestones marked *ledger* were not re-audited here.

| Milestone | State | Notes |
|---|---|---|
| V3-00 baseline | PARTIAL → historical replay gate **verified** | No post-merge expensive v2 heldout rerun (BASELINE.json) |
| V3-01 benchmark harness | PARTIAL (*ledger*) | Consumed-budget runners, frozen manifests; full multi-family heldout incomplete |
| V3-02 orchestrator extraction | PARTIAL (*ledger*) | Legacy selector adapter extracted; full coordinator extraction outstanding |
| V3-03 matched-seed replication | PARTIAL (*ledger*) | v10/v11 implemented and CI-tested |
| V3-04 conditional region portfolio | PARTIAL | No policy promotion; sealed evaluation shows no superiority |
| V3-05 cost acquisition | PARTIAL (*ledger*) | No calibration coverage claim |
| V3-06 surrogate | NOT ATTEMPTED | Optional, killable |
| V3-07 taxonomy | PARTIAL (*ledger*) | Descriptive only |
| V3-08 campaign statistics | PARTIAL (*ledger*) | Fixed-family Holm holdout implemented; population FWER interval not established |
| V3-09 witness and fault containment | Engineering scope **implemented and CI-verified** | Limits: no cgroups, no genuine ENOSPC, v1/v2 store not hardened, no OOM cell |
| V3-10 external comparison | **EXECUTED, negative result** | Aligned genuine SMAC3 and Optuna TPE comparison run once on a fresh cohort; no advantage. ruptures parity is detector-only |
| V3-11 claim audit | This document, **FINAL evidence verdict** | Merge readiness below |

## Release and merge readiness

The engineering gates are met at exact SHAs, and the scientific verdict is final and negative for external superiority. The open items are:

1. Done: merged into `main` as `e7169dd` after CI passed on the PR head `f0501f5`, with owner approval. `main` CI is green at `e7169dd` and `9792e81`. No release has been tagged; any release needs its own exact-SHA gates and owner approval.
2. Development continues on `main` per [`../HANDOFF.md`](../HANDOFF.md).
3. Retain the raw confirmation artifacts durably before their CI expiry on 2027-02-21, if the result is to be cited.
4. The residual risks above remain disclosed, not resolved. A future external claim needs a new, separately frozen protocol on a new cohort, for example with more discriminating, less saturated workloads. This cohort must never be reused.
