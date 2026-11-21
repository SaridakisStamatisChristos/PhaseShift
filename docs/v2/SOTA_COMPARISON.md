> **Status notice (R7 continuation, 21 November 2026):** This file contains historical checkpoint observations; consult [CONTINUATION_STATUS.md](CONTINUATION_STATUS.md) for the current exact-revision evidence and open hard gates. Earlier “pending latest CI” statements describe older revisions, not the current branch head.

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

## Current claim status (21 November 2026)

R7 has added a fixed-family campaign, seed-isolated confirmation, PELT, separate calibrated pilot, and budget replay verification to the draft v2 branch. The last fully verified campaign checkpoint is `c2cb1fb` (58/58 CTests, 95.62% production line, GCC/Clang, ASan/UBSan); later changes **must** pass their own exact-revision CI. The synthetic four-policy cliff/null comparison uses equal candidate ceilings and is not equal total CPU or wall. External SOTA comparisons, independent multi-seed held-out evaluation, and general positive uplift remain **UNVERIFIED**. Do not promote to release or SOTA status.
