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

## F-015 — Rare admissible category invisibility in weighted search

Opt-in v12 conditional policy enumerates declared disjoint rectangle strata lazily and allocates one initial matched legal edge in each, independent of that rectangle's virtual volume. A direct small finite-domain oracle requires both rare/common categories before repeat, and synthetic XOR requires all four categorical contexts. **State: test authored; exact-head CI pending.**

## F-016 — Implication rule silently ignored as if legal

AdmissibleSearchSpace indexes disjoint rectangles but is not a complete oracle for arbitrary conditional implication rules. Until the richer conditional feasibility engine is integrated, v12 refuses source configs with conditional_rules. Both positive disjoint-rectangle run and negative implication test are CI gates. **State: regression authored, exact-head CI pending.**

## F-017 — Mixed structural coordinates change across a matched seed

Independent Python SQLite oracle checks matching category context, legal only-primary-axis shifts, same fresh seed, full raw workers, input digests, and deterministic decision transcript. Tamper a prior evidence digest, arm, coordinate, seed or effect and require verify to fail. Kill before and after pair commit. **State: test authored; exact-head CI pending.**

## F-018 — Compiled conditional rule silently drops a prohibited branch

The independent v13 Python oracle enumerates all 16 (`negative`,`threshold`,`x`) combinations and admits exactly six from the frozen implication rules. It independently checks every persisted matched-pair endpoint, holds non-primary context constant, checks rare stratum coverage, and rejects invalid rule axes. The v13 policy uses the existing canonical v2 rule compiler on run, verify and resume, not an ad hoc or duplicated implication evaluator. Earlier v12 run semantics do not change. **State: tests authored; final CI acceptance pending.**

## F-019 — Conditional region tree silently revisits an edge or excludes a rare cell

Independent finite oracle exhausts 62 legal edge pairs in two disjoint categorical cells, including contrasting double-cliff/xor signs; new edges may not repeat. Every legal context is represented independently before local bisection. Separate 2.048B virtual-domain test caps live contexts and checks no Cartesian allocation. Region-state hash and raw worker evidence must replay under forced crashes, with v12/v13 backward behavior unmodified. **State: tests authored; CI verification pending.**

## F-020 — No-op arm corruption falsifier (v14 first CI)

First exact-head GCC CTest (`f26d5da5`, test 106) failed because the v14 Python tamper test assigned a fixed `replicate` label to an already-replicate pair, so `verify` correctly accepted an **unchanged** transcript. This is a testing-oracle error, not evidence that the verifier accepts a changed policy decision. The repaired falsifier atomically toggles `explore` ↔ `replicate` using SQL `CASE` regardless of the original arm, guaranteeing actual corruption. Re-run GCC, Clang, sanitizer and coverage on the repaired exact SHA before accepting this check. State: REPAIR WRITTEN; new-head results pending.


## F-021 — Cost preference confounds discovery or leaks evaluation observations

V15 cost learning consumes only committed discovery data. The independent fixture reserves every fourth pair for prequential evaluation; check that an extreme heldout cost changes coverage but cannot enter the fitted empirical envelope. Miscoverage, cold start and a censored pair force neutral cost weighting; missing CPU remains missing. A separate real-worker Python reconstruction checks the empirical median/envelope, heldout/training counts, component score and raw CPU totals, then corrupts cost and diagnostic records and tests both crash boundaries. Isolated cost/uncertainty/diversity/interaction/replication score ablations receive deterministic unit falsifiers. This checks engineering behavior, not population coverage or policy superiority. **State: authored; current CI pending.**


## F-022 — Equal pair-count and fabricated benchmark improvements

Predeclared paired v14/v15 real-worker comparison uses actual consumed CPU and wall at fixed pair count, not equal-resource assumptions. Independently reconstruct timing contrasts and CPU from raw samples, reject illegal coordinates, and retain all negative/over-ceiling and failed trials. Require explicit unlabelled truth for real sorting/Top-K. Tampering a scorecard input's consumed CPU by one nanosecond must be rejected by the independent recomputation; preserve prior raw DB and command artifacts. **State:** CTest authored; CI pending.


## F-023 — Rival-shape confusion and sparse projection fabrication

Independent enumeration of all binary length-eight level profiles and all feasible cut masks checks exact v2 DP/PELT score integration. Ground-truth opposing double cliffs, constant, smooth actual-coordinate linear trend, piecewise slope and isolated impulses must classify appropriately or abstain. Missing/censored/variable edges cannot connect unrelated reconstructed profiles. Repeated same-seed pair evidence is distinct from full factorial confirmation; duplicate seeds/forged failed metrics/confirmation are rejected. Read-only real-worker JSON report is reproducible on an immutable snapshot and rejects forged prior policy decisions. **State:** tests authored; exact-head CI pending. No population performance or localization coverage claim.


## F-024 — Adaptive reuse, incomplete families and forged holdout outcomes

Freeze requires complete replayed discovery and disjoint confirmation seeds, explicit fixed member ordering and direction, no duplicate structural edges and immutable family specification. SQL sidecar stores real matched worker evidence; source fingerprint binds all discovery tables except raw bytes which are regenerated and independently checked on replay. Recompute raw log metrics and worker CPU; reject altered source, spec, effect, order, seed and raw CPU. Inject both holdout commit-boundary exits, preserve resume and frozen inventory. Independent binary enumeration over 2^20 signs and Python integer binomial/Holm oracle test exact marginal p-values, fixed family denominator, ties and assumption/censor abstention. Incomplete family reports expose only aggregate progress (raw SQLite can still be read). Declared independence is not an empirical guarantee. **State:** tests authored; exact-head CI pending.


## F-025 — Correlated-null real-worker family stress

Real calibration worker work is checked against an independent integer splitmix implementation; wrong-answer outcomes must have neither elapsed timing nor batch/work fields, and true input-specific fault paths must be identified by the oracle. Preserve full campaign inventory, predetermined selection rule, disjoint heldout seeds, fixed family size and raw membership/contrast reconstruction. Exact integer binomial tail and independent Holm adjust all eligible members. Retain nominal rejections, failures and negatives. Actual bursty contention is a dependence stress, not proof of any particular autocorrelation or null calibration. CI runs four campaigns and rejects a forged scorecard. Population uncertainty is withheld without independent campaign evidence. **State:** implementation/test authored; exact-head CI pending.


## F-026 — V3-09a durable attempt journal and crash boundaries

Exercise real Linux SIGKILL after durable reservation while writer lock is held; competing resume must fail rather than abandon a live writer's attempt. Recovery after precommit exit 88, in-transaction exit 90 and postcommit exit 89 must preserve raw rows atomically and retry exact case/seed identity without duplicating a committed result. Independent SQLite report recomputation must sum only observed committed windows, never ascribe zero as a measured cost to abandoned work, and retain an explicit ACCOUNTING_INCOMPLETE caveat. Read-only verification must not update records. Tampered attempt deletion, proposal identity, missing origin, changed executable digest and altered frozen-source accounting counters are rejected. Not an authenticated log, not a descendant containment test, and no claim that all crash/resource costs are measured. Exact-head verification pending.


## V3-00 historical compatibility falsification (22 November 2026)

Run 44986318671, job 137097561518: baseline and candidate built, both baseline fixture-generation CTests passed, but schema 7 full snapshot after report/replay differed. Code inspection: `report()` deliberately refreshes derived `aggregates` and upserts timestamped `artifacts`; `replay()` uses a read-only Store. The initial test incorrectly required report to be entirely read-only. Repair runs reporting and export on separate copies, requires full original logical snapshot equality after both replays, and checks report-copy authoritative rows and raw report equality. Only the two explicitly derived table row sets are excluded from the report-copy comparison, not from replay. No legacy production behavior changed. Re-execution pending; no gate closure claimed.


## V3-01b independent adversarial gates — 22 November 2026

- F1: smooth change in synthetic operation count must not be relabelled a planted discrete cliff; generated context oracle must enumerate zero planted jump edges. Check `tests/v3/multidim_truth_test.py` and C++ work formula tests.
- F2: a rare positive context must not be silently assumed to cover the full hypercube; union-of-scopes enumeration validates only admissible coordinates. Mutating a scope or context should be rejected before scoring.
- F3: repeated correct edge nominations must not double-count independent discovered edges; wrong-sign nominating a true positive edge is false for signed truth evaluation. Check unit test.
- F4: a pair whose endpoints skip a legal neighbor is rejected, even if its interval contains a true jump; a censored pair cannot contain finite effect. Independent SQLite falsification fixture.
- F5: a missing seed, policy, run.sqlite, raw log, plan SHA, or mismatched exact source config fails replay; run with `--verify-only` after real CLI campaign.
- F6: even if exploratory seed-paired intervals look positive, no cross-engine winner is computable because endpoints and budget protocols differ. The gate rejects apparent score comparability rather than silently pooling outcomes.

No successful execution evidence is asserted here until GitHub Actions for the exact commit complete. Keep raw negative/censored results and the original schema 7/8/9 historical compatibility lane.


## F-027 — Silent holdout re-measurement after interruption

Attack: kill `family-confirm` after the worker measured a holdout pair but before commit, then resume. Under v16 no trace remains, so unfavourable holdout outcomes could be re-rolled at the same frozen seed. Schema-19 falsifiers:

- a real SIGKILL while a durable STARTED reservation is live, with a competing writer that must fail rather than abandon it;
- exits before, during and after commit, each of which must leave exactly one new ABANDONED or COMMITTED row with identical frozen identity;
- an independent reconstruction of every attempt against `holdout_seeds`, the `(seed ^ index) & 1` order rule and the committed `v3_pairs` row;
- rejection of deleted abandoned or committed attempts, uncommitted links, altered seed, order or case identity, extra or beyond-schedule attempts, a missing protocol row and a `user_version` downgrade.

Residual, deliberately recorded: a full journal strip to a valid v16 layout is undetectable without signing (V3-09 signed export remains OPEN).


## F-028 — Forged, re-signed or post-hoc rewritten boundary witness

The attacks are listed in the V3-09c entry of the implementation ledger, and each must be rejected with its asserted error, not merely a nonzero exit:

- an altered witness without re-signing;
- an attacker key with a swapped bundled public key;
- the genuine key re-signing altered evidence (caught by full-replay equality);
- a truncated signature, a byte flip, a wrong or missing trusted key;
- a journal strip, a renumbered abandoned-attempt deletion, a raw holdout CPU edit or a discovery wall-clock edit made after signing;
- a witness checked against a different run.

The signature is independently checked with the `openssl` CLI and both digests are independently recomputed. Residual: evidence rewritten *before* export is signed as-is; the signature attests to replay-consistency at export time, not to the history before it.


## F-029 — Fabricated or cherry-picked witness minimization

The attacks are listed in the V3-09d entry, and each must be rejected for its asserted reason:

- an unsigned edit;
- re-signing with the genuine key after flipping a decision, hiding a failed shrink attempt and renumbering blocks, editing one raw `ns`, changing a seed, lowering the budget, promoting the claim, or dropping a pair;
- the wrong trusted key, an interrupted record, or verifying against a different run;
- a holdout raw-CPU edit after signing.

A shrink that destroys the boundary must be retained as a failed attempt, and the original witness preserved. Minimization must refuse a non-rejected member, so a boundary that failed its holdout cannot be "minimized" into apparent support. Residual risks:

- Acceptance tests are adaptive and uncorrected.
- The minimizer and the verifier reproduction run on the same host.
- A rerun to a new output directory is not linked to an abandoned one.
