# PHASESHIFT v2 — R7 continuation evidence and outstanding gates
Last updated: 2026-11-21. This is an evidence/implementation status, **not** a release or SOTA certificate.

## Frozen target, compatibility and exact CI boundary

Main remains `93b47590c7e44eaf062d97995e9c5cd33274a7a2`; draft PR #4 is intentionally unmerged. The original v1 CLI/default, generator identity, oracle/worker containment, native plugin ABI, SQLite v7 behavior, and sealed holdout remain hard invariants.

The last **all-green exact-revision** checkpoint at the time this file was written was `94ddf552`, after repair of the nested confirmation transaction: build/GCC+Clang, pull-request and push sanitizers, and production coverage were successful. All later changes require independent validation at the final branch SHA. Do **not** infer a final-branch pass from this checkpoint.

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
