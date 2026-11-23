# What it would take for PhaseShift to be SOTA

**Status:** planning document, 2026-11-23. It is not evidence. Every item below is a **hypothesis to test**, not a result. The current verdict is in [`V3_CLAIM_AUDIT.md`](V3_CLAIM_AUDIT.md): **no SOTA, no external advantage established.**

"SOTA" here means what directive §2 and §17 define: a *measured, scoped lead* in **boundary discovery per unit of consumed resource**, on frozen, known-truth, held-out benchmarks, against genuine, correctly configured competitors, with the results confirmed once on fresh data. It does not mean the fastest optimizer or the best change-point detector.

## 1. What the evidence says today

| Evidence | Result |
|---|---|
| One-time sealed internal evaluation (360 trials, synthetic known truth) | V18 / v15 / v14 positive hits 53 / 50 / 52. No superiority; the newer acquisition policies did not measurably beat the older ones |
| Development vs SMAC3 2.4.1 (80 campaigns) | +0.4 nominal rejections per seed cluster (exploratory) |
| Development vs Optuna TPE / random (120 campaigns) | No difference |
| **One-time confirmation vs SMAC3 and TPE** (40 trials, fresh cohort) | H1 5/4/11 better/worse/tied, H2 5/7/8; both NOT SUPPORTED (Holm p = 1.0). The SMAC signal did not replicate |
| ruptures 1.1.10 PELT | Identical penalised objective. Detector parity only, not a lead |

Observations from the confirmation, which are descriptive only:

- **V18 explores more widely.** It visited 140 and 160 distinct legal edges, against SMAC's 84 and 92 and TPE's 129 and 144.
- **That breadth was not rewarded.** Each policy then nominates only its top-2 edges by |effect|, and the endpoint scores just those.
- **The endpoint saturated on `sort_ascending`.** Every policy had 39–40 of 40 nominations confirmed.
- **V18 had the lowest discovery wall time,** mostly because SMAC's Python harness overhead (about 46–53 s) dominates. Costs were not matched, so this is not a resource-efficiency result.

## 2. Diagnosis: why there is no lead yet

These are working hypotheses, each to be tested in development.

1. **The external test used the wrong endpoint for the thesis.** "Reproducibility of the two largest effects" is a max-finding task, which is exactly what SMAC and TPE are built for. PhaseShift's claimed value is finding **all distinct boundaries**, including rare and conditional ones, with controlled false discoveries, per CPU. That endpoint (§2.3 `S_i(b)`, `Y_i(B)`) has never been run against external competitors.
2. **The policy throws away its own advantage.** V18 covers more edges, then collapses to a top-2 nomination by |effect|, so its broader coverage never reaches the scored output.
3. **The workloads are too easy and the budget too small.** Two unlabelled real workloads with a handful of edges and 12 queries leave little room to separate policies, and many seed clusters tie (11 of 20 in H1). The sign test on ties has low power.
4. **The acquisition innovations have not yet shown internal gains** (the sealed evaluation found no v18 > v15 > v14 ordering). The algorithmic levers in v3 are unproven even against PhaseShift's own older policies.
5. **Resource accounting does not match the objective.** Comparisons fixed the query count, not consumed CPU, and per-query process startup is a large share of cost for cheap workers.

## 3. Algorithmic levers to develop (each one falsifiable and ablated)

Order follows expected value. For each lever, the gate is the same: in development on the V4-00 known-truth benchmark (see `docs/HANDOFF.md`), with matched CPU, ablated against the lever turned off, beat the best incumbent **and** the best external on `S(b)` AUC or `Y(B)`, with no worse false-discovery rate on null fixtures. Otherwise kill it.

1. **Multi-boundary allocation instead of top-k nomination.** Screen every distinct edge that clears the practical threshold, then spend confirmation CPU across candidates adaptively, for example by expected information per CPU-second. Report the distinct confirmed boundaries. This directly converts V18's broader coverage into the scored output.
2. **Sequential, anytime-valid confirmation.** Use group-sequential or SPRT-style sign tests with alpha spending in place of fixed 20-pair blocks, stopping early on clear accept or reject. This cuts CPU per confirmed boundary, and the CPU saved buys more coverage. The family-wise guarantee must be preserved and validated on null campaigns (V3-08 infrastructure).
3. **Boundary-aware acquisition.** Bisect toward the edge once a cliff is bracketed (the v2 cliff-bracket and region-tree machinery), do rare-region and conditional-context splitting (V3-04), and use cost-aware scoring calibrated on **consumed** CPU (V3-05 is still open).
4. **Localization as a scored output.** Report boundary intervals with uncertainty, reusing the v2 DP/PELT and the V3-07 taxonomy, and score localization error `E_i`. Generic optimizers do not produce this output.
5. **Amortized measurement for cheap workers.** Add a persistent-worker or batched-pair mode, offered to **all** policies alike, so process startup does not dominate. It is a fairness and efficiency lever, not an advantage by construction.
6. **Optional learned surrogate (V3-06).** Try this only if levers 1–4 plateau and the development evidence specifically motivates it.

## 4. A benchmark that can show a lead, if one exists

- **Known truth first.** Use the multidim synthetic families (multi-context steps, sparse multi-step, rare hidden cliff, masked interactions), where `multidim_truth.py` enumerates the true jump edges. Grow them toward harder regimes: more edges, rare islands, higher-dimensional conditional legality and heteroscedastic noise. Keep the null and smooth controls (`smooth_no_jump`, `multidim_null`) for the false-discovery rate.
- **Semi-labelled real workloads.** Add algorithm pairs whose crossover is analytically bracketed (for example insertion vs merge sort over n, or heap vs full sort over k for Top-K). Truth there is "lies in this interval", scored only by interval agreement and never called exact truth.
- **Matched consumed resources.** Every policy stops at the same consumed worker CPU (and, separately, host wall). Optimizer, harness and startup overhead are reported separately. Show the whole `S(b)` curve, not one budget point.
- **Held out by family.** Development and evaluation families are disjoint, and each gets a fresh derived-seed-disjoint cohort. Evaluation data is never used for tuning.

## 5. Competitors

The genuine, pinned and correctly configured ones are already integrated: SMAC3 2.4.1, Optuna 4.6.0 TPE, random, and PhaseShift's own v14, v15 and V18 policies.

Add more, but only as genuine implementations:

- **grid search**, the natural baseline for low-dimensional boundary search;
- **an active level-set / boundary-estimation method** (for example a GP-based level-set estimator from a maintained library), the closest prior-art competitor for "find the boundary";
- **ruptures**, kept as the detector-only stage comparator.

Fuzzers (PerfFuzz, AFL++, SlowFuzz) belong only where a shared target and objective exist, and are scoped separately. Every competitor's failures and setup costs are retained.

## 6. Statistics and protocol for a defensible claim

- **Power first.** Size the confirmation cohort from the development effect size and tie rate. The 20-cluster sign test with 40–55% ties had little power. Prefer a continuous primary endpoint (paired `S(b)` AUC difference), tested by an exact paired permutation or sign-flip test, with more clusters if needed.
- **Pre-register everything:** primary endpoint, margins (directive §2.4 proposes ≥20% `S(b)` AUC improvement or ≥20% less cost to the first true boundary), the multiplicity scheme across families and competitors, a non-inferiority margin for null false discoveries, and the `INCOMPLETE_NO_CLAIM` rules. Freeze before any evaluation observation.
- **One-time confirmation** on a fresh cohort, with the owner's approval, exactly as V3-10 was done: pinned binary, independent audit, byte-identical recomputation and archived evidence.
- **Replication.** Repeat across sessions, and across at least two host classes if any hardware-general wording is wanted.

## 7. Engineering prerequisites

1. **V3-02 coordinator extraction**, so that a winning policy can be promoted into the main `explore` CLI rather than living only in `phaseshift_v3_pairs`.
2. **Complete accounting:** an optional cgroup v2 harness, persistent `contained_descendants` counts in SQLite, and integrity checks for the v1/v2 store.
3. **Durable artifacts:** store full raw evidence for any claimed result as a release asset with SHA-256, not only as 90-day CI artifacts.

## 8. Stop rule and honest fallback

If a reasonable development programme (levers 1–4 above, each ablated) produces no lever that beats the best incumbent and the best external on the §2.3 endpoint, record `SOTA_NOT_ESTABLISHED`. PhaseShift then stays what it verifiably is: an auditable, falsifiable boundary-discovery engine with independent oracles, signed witnesses and honest accounting, and the best verified policy is promoted only for the regime where it measurably helps. A claim is allowed only in the directive §17 template form:

> "At source revision `<sha>`, PHASESHIFT achieved `<measured outcome>` relative to `<named baselines>` on `<frozen matched benchmark>`, under `<CPU/wall budget>` and `<statistical protocol>` … This result does not claim superiority on different objectives or untested hardware."

## 9. Ordered plan

| Step | Content | Gate |
|---|---|---|
| 1 | V4-00 known-truth external benchmark (development) | Contract tests + CI; development scorecard recorded |
| 2 | Levers 1–2 (multi-boundary allocation, sequential confirmation), ablated | Development `S(b)`/`Y(B)` gain with no null-FDR regression, or the lever is killed |
| 3 | Levers 3–5 as indicated by step 2 | Same |
| 4 | Harder known-truth families, semi-labelled real workloads, extra competitors | Frozen development and evaluation family split |
| 5 | Power analysis, pre-registration, one-time confirmation (owner approval) | Independent audit; byte-identical recomputation |
| 6 | Session and host replication; V3-02 promotion of the winning policy | All CI gates on the release SHA; owner approval |
