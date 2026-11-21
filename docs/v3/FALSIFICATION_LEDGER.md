# PhaseShift v3 adversarial falsification ledger

## F-001 — Equal ceilings are not equal consumed budget
Observed in v2 architecture: the dual-ceiling runner pauses after one case, but its old summary is per-run and does not give a complete discovery success-by-consumption curve. V3-01 must reject claims of equal resource consumption based solely on equal limits. **State:** OPEN, explicit regression test required.

## F-002 — False speed promotion from wrong answers
Status must never be represented as a finite winning speed. V3-01 independently rejects failed rows as valid confirmed discoveries. **State:** OPEN.

## F-003 — Synthetic threshold treated as independent real-world discovery
Calibration threshold 24 is explicitly planted by the existing v2 generator. Its truth is suitable for benchmark self-tests, not evidence of novel real-world findings. **State:** OPEN.

## F-004 — Retrofitting holdout after policy tuning
Training/evaluation manifests must be distinct before selecting policy weights; exact manifest hashes and frozen seeds must be persisted. **State:** OPEN.

## F-005 — Process-tree accounting gaps
Linux waited-child RUSAGE includes reaped descendants but not necessarily detached subprocesses; any v3 source must label this limitation rather than claim complete worker CPU. **State:** OPEN.

Update each entry with exact SHA, reproduction command, actual results and repair. Do not delete failed attacks.

## F-006 — Selector extraction changes legacy RNG/case schedule
Attack: compare direct original policy-arm implementations against extracted adapter over fixed seeds, feedback, invalid proposals, RUSAGE budgets and acquisition origin. `tests/v3/selector_characterization.cpp` is the independent direct reference. **State:** test written; current-head CI not yet fully observed.

## F-007 — Conditional virtual-space blowup
Attack: enumerate small legal mixed/categorical spaces independently, then declare a legal 2.048B-candidate universe and measure bounded RSS. `tests/v3/axis_slice.cpp` asserts legal unique proposals, discovery-only input, deterministic replay and no virtual-sized allocation. **State:** test written; current-head CI pending.

## F-008 — Policy feedback learns from sealed holdout
Attack: supply `Cohort::Confirmation` to either the v3 snapshot or ordered-axis policy and assert hard rejection without consuming the pending proposal. **State:** tests written; end-to-end provenance enforcement outstanding.

## F-009 — Local mid-gap pool masks an unsampled legal point
Current ordered-axis midpoint heuristic examines nearby legal coordinates, then falls back to globally unbiased sampling if those are exhausted. This is a bounded but potentially suboptimal heuristic: nested forbidden/issued midpoint candidates can delay some legal gap points. **State:** explicit performance hypothesis; implement adversarial sparse-gap development ablation before promotion.

## F-010 — Raw SQLite main-file hash silently excludes committed WAL state
**Discovered in real CI:** `793acb69` GCC/Clang/coverage CTest #95 failed at `protocol.py:97`: `sha256_file(run.sqlite) != row["sqlite_sha256"]` for a fully completed calibration worker run. Confirmed independently: a committed SQLite WAL changes its main-file bytes upon checkpoint without changing logical observations. **Root cause:** a byte hash of the WAL-mode main DB is not a stable digest of authoritative committed SQLite evidence. **Repair:** `56ea0b71`, read-only `sqlite3.backup` snapshot digest with explicit connection cleanup and WAL→checkpoint regression. **State:** reproduced and repaired; exact-head CI re-verification required.

## F-011 — Wrong-answer timing injected into new v3 slice scorer
**Attack:** feed a finite effect together with `wrong_answer` status. **Repair:** `49773d24`, explicit non-`ok` status rejection and discovery-only cohort gate before pending-proposal consumption; adversarial unit test. **State:** implemented, current-head CI verification required.


## F-012 — Adaptive decisions forged or influenced by holdout

v11 replay rebuilds explore/replicate choices from raw committed discovery data and validates stored arm, numeric endpoints, seed, source digest and predecision committed-state hash. Proposed falsification mutates each field separately; confirmation cohort feedback must be rejected. **State: code/tests authored; CI pending.**

## F-013 — Seed-mismatched cliff illusion

The independent synthetic oracle supplies common ±0.7 log-time seed noise to two structures with a fixed 0.1 actual effect. Proper matched seeds cancel noise; neighboring mismatched seeds spuriously generate >0.4 differences in all 40 constructed pairs. No claim is made about universal generator exchangeability. **State: test authored.**

## F-014 — v11 adaptation accidentally mutates v10

Implement v11 only for new opt-in run directories; retain v10 fixed-pair runner and previous source Store schemas unchanged. Run both independent CLI checks under CI. **State: tests authored; CI pending.**