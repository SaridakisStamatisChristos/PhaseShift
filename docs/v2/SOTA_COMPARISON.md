# PHASESHIFT v2 — SOTA Comparison (open; no SOTA claim)

The R7 directive explicitly separates implementation, independent correctness, reproducibility, empirically better performance and leadership against external competitors.

| Comparator | Shared objective | Actual external execution | Verdict |
|---|---|---|---|
| Original PHASESHIFT v1 adaptive | Controlled seeded calibration; candidate selection | Small CLI benchmark harness added; final results pending GitHub CI | **Not established** |
| Uniform v2 versus hierarchical v2 | Search cost, localization and confirmed yield | Deterministic unit fixtures only so far | **Not established** |
| PELT / ruptures | Fixed-sequence changepoint detection quality | Not executed; standalone exact DP exists | **Pending** |
| SMAC3 | Black-box mixed-space acquisition under same objective | Not executed; adapter/compatible budget absent | **Pending** |
| PerfFuzz | Performance-input stress, not arbitrary boundary inference | Not executed; objective not directly equivalent | **Not comparable without adapter** |
| SlowFuzz | Adversarial computational cost maximization | Not executed; objective not directly equivalent | **Not comparable without adapter** |
| PerfGen | Analytics benchmark generation | Not executed; shared target yet to be defined | **Pending** |

The master plan's >=20% time-to-confirmation and >=15% localization/recall targets remain **hypotheses**, not results. No claim of "beats all competitors" or "SOTA" is supported.
