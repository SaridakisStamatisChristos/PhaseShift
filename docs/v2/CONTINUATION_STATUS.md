> **Current-state notice (22 November 2026):** PR #5 has been merged; v2 baseline `main` is `279a8c65ad7ec3741a727ff49aa7f84e0628a6f1` at this checkpoint. The original exact-main jobs passed 93/93 CTests per lane and 95.19% production line coverage. See [v3 baseline provenance](../v3/BASELINE.json) and [historical research scorecards](../v3/V2_HISTORICAL_SCORECARDS.json). Statements below are original historical checkpoints, not live PR/branch status. Expensive research was observed on its earlier PR merge checkout, not established on exact merged main.

> **Current status (21 November 2026, PR #5):** PhaseShift v2 was merged into `main` as `8e795197b65af9ac73abff84bfaaefe29d6c92fb` (PR #4). New R7 completion work is in **draft PR #5**, not merged. The status and checklist in [R7_COMPLETION_CAMPAIGN.md](R7_COMPLETION_CAMPAIGN.md) supersede older draft/old-main statements below, which are preserved solely as historical evidence. The experimental code needs exact-SHA CI and held-out performance verification; no SOTA claim.\n\n# PHASESHIFT v2 — R7 continuation evidence and outstanding gates
Last updated: 2026-11-21. This is an evidence/implementation status, **not** a release or SOTA certificate.

## Frozen target, compatibility and exact CI boundary

Main remains `93b47590c7e44eaf062d97995e9c5cd33274a7a2`; draft PR #4 is intentionally unmerged. The original v1 CLI/default, generator identity, oracle/worker containment, native plugin ABI, SQLite v7 behavior, and sealed holdout remain hard invariants.

An earlier **all-green exact-revision** checkpoint was `94ddf552`. A newer fully passing code checkpoint is `60be88bd` (see the 21 November continuation record below). Any later revision, including this documentation update, still requires independent exact-SHA verification.

GitHub CI events: https://github.com/SaridakisStamatisChristos/PhaseShift/actions

## Verified milestones and newly implemented experimental slices

| SHA | Changes | Evidence boundary |
|---|---|---|
| `163e83e` | Exact inverse of mixed-radix and disjoint admissible subspace coordinates, no product materialization; reject unknown/noncanonical coordinates | All five exact-SHA GitHub workflows green |
| `e7bb3e9` | Opt-in `hybrid_mixed_v2`: matched nuisance/replicate context, interval refinement on numeric primary, categorical matched-level coverage; deterministic PRNG replay | GCC/Clang, production coverage, PR sanitizers green on exact SHA |
| `45c7ad9` | Transactionally couple fixed-sample seal, raw positive verdict and campaign sign-flip p-value; inject rollback in campaign test | Initial CI falsified due nested `BEGIN`; superseded by `94ddf55` |
| `cea1667` | H2..H16 witness outputs isolated as `witnesses/Hn/`; v1/H1 paths preserved | Initial compile falsified undeclared path; superseded by `dc7d197` |
| `fbd466b` | Linux SIGKILL after each of three evidence write points before COMMIT, then verify no partial rows and exact retry | CI pending after later repairs |
| `dc7d197` | Repair failure-witness artifact directory declaration | Subsequent exact-SHA CI required |
| `94ddf55` | Convert seal's nested `BEGIN` to safe, standalone/outer-transaction-compatible SAVEPOINT | **All five workflows passed** |
| `6017d6e` | Reject v2 confirmation seed windows that exceed signed SQLite range before TESTED; disjoint H1..H16 boundary tests | CI pending full matrix |
| `a779c0e` and `dcf5e4d` | Opt-in bounded actual-coordinate piecewise-linear slope model with brute oracle and discovery-only worker telemetry; fix unique fixture and large-coordinate arithmetic | CI pending |
| `ab54f9f` | Real-worker reproducible 4-policy, 2-fixture, sampled-CPU-ceiling benchmark, persisted SQLite audit, immutable frozen plan and zero-claim scorecard | CI pending |

## Hard gates not yet closed

| Gate | State | Work remaining |
|---|---|---|
| M0 | PARTIAL | Frozen SOTA benchmark completed under actual comparable resource objectives |
| M1 | PARTIAL | Indexed persistent evidence/model cache, full parent-worker CPU and end-to-end wall accounting |
| M2 | PARTIAL | Validated weighted/conditional nonrectangular feasible-space sampling and large-scale RSS gates |
| M3 | PARTIAL | Calibrated uncertainty, adaptive replication, empirical hybrid acquisition gains |
| M4 | PARTIAL | Independently confirmed conditional 2x2 interactions and higher-order context effects |
| M5 | PARTIAL | Noisy holdout study of slope/changepoint models, reliable detection/localization CIs |
| M6 | PARTIAL | Quantified drift and repeatability versus baseline measurement protocol |
| M7 | PARTIAL | Whole-system calibrated null/FWER assessment and selection leakage audit |
| M8 | PARTIAL | Conditional multi-boundary witness minimization and exact fresh independent rechecks |
| M9 | PARTIAL | Additional model/cache persistence and full process termination/restart replay coverage |
| M10 | PARTIAL | Equal **complete** CPU+wall resource budgets; 20+ held-out runs; real comparators e.g. ruptures/PELT, SMAC3 when objectives match |
| M11 | INTERMEDIATE VERIFIED | Latest **final exact SHA** full build, all CTests, >=95% production line, ASan/UBSan |
| M12 | PARTIAL | Public reproducibility package with complete negative and positive results; scientifically justified SOTA verdict |

Synthetic calibration is not a performance frontier. An executed policy comparison with a shared sampled-worker CPU ceiling still permits one complete-case overshoot, does **not** equalize full process CPU or end-to-end wall, and cannot establish SOTA. No verified >=20% time-to-confirmation or >=15% localization uplift is claimed.

## Experimental challenge and run commands

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure

# Diagnostic only, 20 prespecified seeds by default, no output overwrite.
python3 benchmarks/v2/sampled_cpu_challenge.py \
  --binary ./build/phaseshift --out /tmp/ps-v2-sampled-cpu \
  --cases 32 --sampled-cpu-ns 2500000
```

The script records both positive and negative results, failures, raw case/sampled-CPU ledgers and end-to-end wall observations. Do not label as an equal-total-CPU/wall benchmark.

## R7 adversarial repair ledger

- **F-v2-003 / nested transactions:** `45c7ad9` wrapped seal/status/p-value in an outer transaction, but `Store::seal_confirmation` also called `BEGIN IMMEDIATE`. GCC and Clang compiled, but 10+ real confirmation CTests failed with `SQL: cannot start a transaction within a transaction`. Repair `94ddf55` uses a nested SQLite savepoint while preserving independently atomic seals; **all five workflows pass** at repaired SHA. Never revert to separate successful writes.
- **F-v2-004 / witness scope:** `cea1667` introduced H2 path isolation but referenced a branch-local `artifacts` before declaration. Both compilers rejected the code; `dc7d197` repairs the declaration and path check. Latest exact-SHA validation pending.
- **F-v2-005 / seed arithmetic:** Signed SQLite persistence could overflow for large seed values and H2..H16 disjoint windows. `6017d6e` checks the entire fixed holdout+negative-control envelope before persisting TESTED state. Independent upper-bound/disjointness tests added.
- **F-v2-006 / slope oracle ambiguity:** Two equivalent piecewise-linear cuts in the original synthetic fixture made an exact boundary-index assertion underdetermined. Replaced with a unique cut and promoted actual-axis difference calculation to long double to avoid signed subtraction overflow. CI pending.

## Further continuation after the initial status snapshot

- `f68f158`: integrated H1/H2 minimization fixture exercises two independently sealed hypotheses and verifies H1's legacy witness filenames coexist with `witnesses/H2/` artifacts. This tests actual minimization, not family-wise evidence.
- `adbdcb6`: streamed per-case/algorithm early-versus-late block drift diagnostics into v2 report summaries. Groups need at least six valid distinct blocks; the 20% relative flag is a **descriptive heuristic**, never a p-value or reason to declare a performance win. Raw evidence is unchanged.
- `a1b4f39`: adversarial CI repair for strict GCC/Clang rejection of an implicit long-double-to-double slope derivative; range check and explicit cast before telemetry publication. Earlier slope commits failed compilation, so do not accept their CI.
- `2e827e9`: PR and main continue running GCC/Clang and sanitizers; duplicate feature-branch push workflows removed from the triggers to reduce future queue pressure. No weakened test or coverage gate.

**Verification:** all five workflows green at `94ddf55`; later current head exact-SHA CI still pending at this document revision. Do not merge PR #4 or publish SOTA claims without current-head validation, independent held-out outcomes, full CPU/wall fairness, and external matched comparators.

## R7 continuation checkpoint — 21 November 2026, verified code revision `60be88bd`

**Source parent:** `60be88bd33d7b4aad22a2e938bd9759960f4c2b6`; `main` remains `93b47590c7e44eaf062d97995e9c5cd33274a7a2`; draft [PR #4](https://github.com/SaridakisStamatisChristos/PhaseShift/pull/4) is **not merged**. This section supersedes older “latest CI pending” assertions *for the specified code revision only*.

| GitHub-hosted workflow | Exact revision result |
|---|---|
| [build-test 44889672006](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44889672006) | GCC Release 73/73 CTests and Clang Release 73/73 CTests; successful jobs including CLI acceptance |
| [sanitizers 44889671976](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44889671976) | ASan/UBSan 73/73 CTests, workflow successful |
| [production-line-coverage 44889671971](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44889671971) | 73/73 CTests; 4902/5154 instrumented production lines, **95.11%**; passes 95% gate |
| branch coverage | **Not newly measured** by this evidence; no branch-coverage claim |

### New implemented code, falsification and repairs

- `1cb45b3` **F-v2-007:** GCC/Clang/ASan builds on `b6e53bc` failed because the slope oracle declared `std::size_t` and `double` in one `auto` declarator; split declarations. The repaired revision compiled and proceeded to 73 CTests.
- `448ca90` and `e3d2d18`: `hybrid_mixed_v2` now has a **discovery-only** bounded heuristic that uses a measured matched-contrast to request a *fresh common replicate seed* at both endpoints. The second endpoint proposal is issued after the first has been recorded; exact deterministic replay, no-contrast abstention, and paired-seed identity have tests. **Not calibrated uncertainty, an anytime-valid test, or a guarantee of increased yield.** Existing legacy v1 policy remains unchanged.
- `c5495c4` **F-v2-008:** after the compiler fix, a real multi-hypothesis minimization test failed (72/73) because the synthetic fast path still wrote H2 witness files over the legacy H1 directory. Synthetic H2..H16 files now use `witness_output_root`; overwrite is refused. Regression now passes.
- `164871e`, `7fbb059`, `60be88b`: bounded **bundle schema 2** adds export, verify and import of `witnesses/H2..H16/` with member SHA-256, path/symlink rejection, persisted witness-to-CaseID input-digest matching, and mutation/tamper tests. Existing flat **schema 1** and H1 remain supported.
- `164871e` **F-v2-009:** CI caught accidental multicharacter `'\\n'` / `'\\r'` literals in a new checksum-cleanup loop; `60be88b` corrected them to actual newline/carriage-return character literals, passing exact-revision CI.

### Current R7 hard requirements and evidence states

| Milestone | State | Residual contract |
|---|---|---|
| M0 | PARTIAL | Four-policy frozen diagnostic exists; full matched-budget and broad gold-standard families absent |
| M1 | PARTIAL | Explicit v2 opt-in and persisted decisions present; full indexed evidence cache/parent overhead accounting absent |
| M2 | PARTIAL | Virtual mixed-radix, admissible rectangular union, seeded sampling, resume, and huge-space oracle exist; weighted/nonrectangular valid candidates and measured RSS target open |
| M3 | PARTIAL | Hierarchical, stratified, hybrid and paired-seed heuristic exist; validated uncertainty, cost calibration, and superior held-out yield absent |
| M4 | PARTIAL | 2×2 matched-seed interaction screening exists; independent conditional confirmation/more contexts absent |
| M5 | PARTIAL | Exact DP, PELT fallback and slope models exist; noisy independent held-out localization/taxonomy tests incomplete |
| M6 | PARTIAL | Opt-in calibrated worker and descriptive drift diagnostics exist; repeatability/precision gains not established |
| M7 | PARTIAL | Frozen SQLite/ Holm family, H1/H2 holdout, and sealed evidence exist; broad null/FWER calibration open |
| M8 | PARTIAL | H1 and H2 witness isolation and bundle round-trip verified; conditional/multi-boundary minimization with fresh independent rechecks incomplete |
| M9 | PARTIAL | Additive SQLite versions, atomic decision/CPU ledgers, kill tests exist; comprehensive model persistence/restart matrix open |
| M10 | PARTIAL | Real-worker sampled-CPU-ceiling diagnostic only; **not equal whole-process CPU and wall**, 20+ held-out seeds, or external matched comparators |
| M11 | VERIFIED ON `60be88b` | Exact GCC/Clang/sanitizer/coverage runs all green, 73 CTests each; this documentation commit awaits its own checks |
| M12 | PARTIAL | Source/evidence docs available; final defensible SOTA verdict and comparative release evidence missing |

**Scientific claims:** no measured >=20% time-to-confirmation or >=15% localization uplift; no executed comparison establishing SOTA. The code remains a **draft research branch**. No merge, license change or release is authorized. Preserve any negative results and avoid promoting experimental v2 policy to v1 default.


## R7 continuation — weighted lazy search and atomic phase-budget evidence (21 Nov 2026)

**Code checkpoint:** `1db41335a1b14f34a3330b9c1e7c19415f2821f0` on draft feature branch. This checkpoint is pending its own complete GCC/Clang/sanitizer/coverage results. Do not inherit `aacddb0`'s green status. Main stays `93b47590c7e44eaf062d97995e9c5cd33274a7a2`.

| Milestone | R7 state | Concrete implementation / open gate |
|---|---|---|
| M2 weighted priors | IMPLEMENTED, exact-SHA CI PENDING | `weighted_v2.hpp`, `search.domain_priors` (positive finite 1e-6..1e6, unspecified labels weight 1), opt-in `weighted_v2` only; weighted sampling without replacement over disjoint admissible rectangles with sparse prefix-mass ledger and replica seeds; O(sum axis labels + stored prefix history) memory, never enumerates full Cartesian product |
| M2 adversarial validation | PARTIAL | 1e9 virtual-grid property case, seeded no-duplicate/exhaustion, prior bias statistical calibration fixture and 17 fail-closed config mutations; no measured huge-grid resident RSS gate, no arbitrary nonrectangular feasibility expression |
| M1 case phase accounting | IMPLEMENTED, exact-SHA CI PENDING | Each completed v2 case transaction now persists `v2_phase_budget` with scheduler-proposal wall, worker-invocation wall, valid timed CPU sample ns and completion flag, linked by the same transaction's decision index; `tests/verify_v2_weighted.py` compares pause/resume vs uninterrupted order and raw per-case event linkage |
| M1 budget inference | PARTIAL | New event durations are *not* direct parent CPU or full-process-tree CPU; not an equal budget research claim |
| M3–M10, M12 | PARTIAL | All earlier open requirements, in particular calibrated uncertainty, independent conditional confirmation, complete CPU+wall parity, 20+ heldout repetitions and external comparators, remain open |

**R7 falsification log:**
- F-v2-010: new C++ weighted unit test used an enclosing automatic variable as a lambda default parameter, rejected by GCC/Clang/ASan compile. Repaired in `9bcd6f1` without weakening assertions; follow-on CTest exercised weighted real worker.
- F-v2-011: the first weighted SQLite replay checker queried nonexistent `next_rng_state` rather than schema-defined `rng_state`. The actual 12-candidate run completed in uninterrupted and resumed modes; its *independent* replay SQL audit stopped at the invalid query. Repaired in `1db4133`; verification on that exact SHA still required.
- The append-only cost record `397d12b` adds independent per-case timing tags, not inferred CPU equality; `e6bee64` adds invalid-schema/legacy coercion tests. Neither constitutes independent scientific uplift.

**Claim ledger:** no numeric SOTA uplift, no 20% time-to-confirmation improvement, no 15% localization gain, no globally calibrated false-positive rate, and no equality of whole-system CPU/wall claimed. The previous fully green exact-sha handoff was `aacddb0` before this feature slice. PR #4 remains DRAFT and NOT MERGED.
