# PHASESHIFT v2 — R7 implementation ledger

Baseline main: `93b47590c7e44eaf062d97995e9c5cd33274a7a2` on 2026-11-21. Existing push CI `build-test`, `sanitizers` and `production-line-coverage` succeeded on **this original main SHA only**.

## Hard invariants

Preserve historical generator bytes/IDs, v1 CLI behavior, SQLite v7 evidence, native plugin ABI, independent correctness oracle, sealed holdout, worker containment, and >=95% LLVM production line coverage. Use explicit v2 opt-in. Do not claim new inference or SOTA without executed equal-budget evaluation.

## R7 requirement / claim ledger

| Requirement | State | Evidence or gap |
|---|---|---|
| G0 main pinned | VERIFIED | SHA above and successful original push CI |
| M0 frozen SOTA benchmark contract | PARTIAL | Scenario inventory only; equal-budget runs outstanding |
| M2 lazy mixed-radix index and uniform sparse sampler | PARTIAL | Opt-in uniform_v2 CLI+resume added, CI verification pending; conditional subspaces/replication remain unimplemented |
| M1 explicit policy version | PARTIAL | v1 default, v2 opt-in; cached evidence/budget ledger pending |
| M5 multi-change model | PARTIAL | Exact K-bounded DP and exhaustive brute-force differential unit test added; no v2 detector routing, PELT, noisy held-out validation or confirmation ranking |
| M3–M4, M6–M12 | UNVERIFIED | Implementation pending |
| G1–G14 v2 hard gates | UNVERIFIED | No branch CI or held-out comparison yet |

Research claim: **none**. Any synthetic fixture is a correctness calibration, not scientific novelty. No PR merge, license or release is authorized.
