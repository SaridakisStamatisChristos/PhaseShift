# PHASESHIFT R7 — Post-audit v1.0 repair ledger

> **Verified closure (2026-11-20):** Repair source SHA `6e343b8f1efec1ca279d0c2eff95bbead6ee0912` passed [GCC/Clang CI](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44756583264) and [ASan/UBSan CI](https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44756583308). GCC 31/31, Clang 31/31, ASan/UBSan 31/31; three CLI smoke passes. The entries below record the original repair execution and pending-state history before these CI outcomes.


Date: 2026-11-20. Contract: [`PHASESHIFT_SPEC.md`](PHASESHIFT_SPEC.md), original PHASESHIFT R7 spec v1.0. The previous immutable `main` SHA `326863e92c6e5abcc70089c3996e556d29c3b9aa` passed 25/25 GCC, 25/25 Clang, 25/25 sanitizer tests plus three CLI smoke scripts, but the independent R7 audit correctly found specification defects. That prior green CI **does not** verify this revised source.

The following describes remediation **source + targeted local tests**, not a claim of remote CI success on the new revision. R7 final gate: full GCC/Clang/ASan-UBSan tests and sanitized CLI smoke at the exact new SHA.

| Finding | Repair surface | Independent acceptance / evidence | Status before new CI |
|---|---|---|---|
| F-001 Direct unearned `CONFIRMED` | `src/store.cpp`, `include/phaseshift/store.hpp`, `src/experiment.cpp`, `src/scaling.cpp`, SQLite v7 `confirmation_evidence` and `confirmation_seals` | `confirmation_integrity`: refuses direct promotion, zero cases, 19 paired groups and discovery/holdout overlap; accepts 20 distinct seed-paired cohorts with exact linked case IDs and sample IDs. `demo` and `failure_boundary_confirmation` re-executed. | Local regression passed, CI pending |
| F-002 Scaling discovery labeled holdout | `src/scaling.cpp` discovery loop passes `false`, holdout uses `true` | `scaling_fingerprint` + `scaling_provenance` query real SQL and require disjoint case populations | Local regression passed, CI pending |
| F-003 Wrong `holdout_sizes` | `src/scaling.cpp` serializes actual new-size vector | `scaling_provenance` checks reported sizes against persisted holdout cases whenever a candidate has holdout observations | Local conditional test passed, positive real-scaling candidate dependent on workload |
| F-004 Unsupported metric silently accepted | `src/experiment.cpp` parses/restricts measurement keys; only implemented `wall_ns_per_operation` admitted | `cli_contract_falsifiers` denies CPU-metric and unknown science-key config | Local regression passed |
| F-005 Plugin-family mismatch | `src/experiment.cpp` rejects ABI-v1 native plugin except `integer_array` | `cli_contract_falsifiers` rejects graph/plugin input | Local regression passed |
| F-006 Unknown flags, ignored `--db` | `apps/phaseshift.cpp`, `src/experiment.cpp`, `include/phaseshift/experiment.hpp` | `cli_contract_falsifiers` rejects malformed, duplicates, unknown arguments; natural smoke verifies explicit existing `--db`, rejects nonexistent path | Local regression passed |
| F-007 B07/B10/B11, research scorecard | `configs/b07_zero_one_bfs.json`, `tests/run_b07.cmake.in`, `tests/research_campaigns.cpp`, `src/experiment.cpp` | B07 actual worker timing + correct oracle on {0,1} graphs; B10 deterministic deceptive scaling synthetic discovery and fresh-size/seed holdout with null/wrong-direction rejections; B11 40 simulated nonstationary ABBA/BAAB nulls with zero *simulated* false confirmations; null and real/synthetic ablation scorecards track yield, null false positives, CPU/wall cost, and witness size when measured. | Local tests passed; **no physical-noise certification** |
| F-008 External binary provenance | `src/store.cpp` | `native_plugin_fingerprint`: `.so` SHA-256 in algorithm version record; rejects changed same-path binary. Compilation flags of foreign binaries unavailable and explicitly unverified. | Local regression passed |
| F-009 Outdated CI status/docs | README, `docs/release_checklist.md`, `docs/v1_requirements_evidence.md`, this ledger | Earlier main Actions #21 documented as historical; revised branch CI deliberately pending until observed | Docs corrected for known evidence |

## Integrity invariants and caveats

The sealed v7 evidence contains case ID, exact sample ID, primary algorithm, side, seed cohort, and a frozen hypothesis link. Sealing requires `TESTED`, at least 20 complete independent seed-paired cohorts, and no case ID shared with discovery. A positive verdict without a seal is rejected. A new-size scaling cohort additionally requires two previously unmeasured distinct sizes on each side. `NOT_CONFIRMED` remains a valid outcome without a complete seal. Legacy v6 positive rows remain historically intact but are reported as `UNVERIFIED_LEGACY_CONFIRMATION` until separately attested; they are not retroactively promoted.

The B10/B11 campaigns use explicitly engineered simulator functions. They calibrate conservative detection logic, but are **not** empirical discoveries about underlying algorithms or proof of controlled false discovery under arbitrary physical CPU noise. Native code execution is still fault containment for trusted plugins, *not* a malicious-plugin sandbox. Equal requested case budgets are reported alongside actual CPU and wall time rather than pretending every search strategy consumes equal CPU seconds.

## Final execution commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
bash scripts/acceptance_smoke.sh

cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang -j2
ctest --test-dir build-clang --output-on-failure

cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPHASESHIFT_SANITIZE=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 PHASESHIFT_BIN="$PWD/build-sanitize/phaseshift" bash scripts/acceptance_smoke.sh
```

Research CI now uploads B07 raw timings, B10/B11 synthetic control outputs and the four-strategy negative-control ablation scorecard as named, commit-linked artifacts retained for 14 days when the jobs execute successfully. This upload step is not evidence that CI has executed yet.

No merge, tag, publication or license change is implied by these repairs.

## F-010 — GitHub CI process reaping race detected after publication

The first GitHub-hosted GCC job on repair SHA `f006b6d90e9c0c414cbca7930cc6fbcfc497a6d6` passed 30/31 CTests; `core` failed its intentionally crashed plugin fixture with `segfault must be isolated and marked signaled`. The parent was calling `waitpid(...,WNOHANG)` once immediately after pipe EOF, then unconditionally killing an exiting worker when the exit status was not *yet* observable. The result could be labeled `infrastructure_error` or `timeout` instead of the real signal.

Repair: bounded 150 ms natural child-exit grace with 5 ms polls before an explicit timeout kill, plus 12 independent fault/reap attempts in the core test. This preserves the deadline and ensures SIGSEGV remains a signaled failure rather than a benchmarkable sample. This incident invalidates any pre-repair claim of fully green remote CI; the revised SHA must pass complete GitHub-hosted GCC, Clang and sanitizer workflows before closure.

## Final R7 exact-revision verification record

- Source commit: `6e343b8f1efec1ca279d0c2eff95bbead6ee0912`, fast-forwarded to `main` after GitHub-hosted verification.
- Build & Test workflow: https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44756583264 — GCC 31/31, Clang 31/31, respective CLI smoke PASS; research artifacts uploaded.
- Sanitizers workflow: https://github.com/SaridakisStamatisChristos/PhaseShift/actions/runs/44756583308 — ASan/UBSan 31/31, sanitized CLI smoke PASS.
- Main audit counterexamples F-001–F-010: implementation repairs and regression tests are incorporated in the verified SHA. The worker process-reaping race was independently exercised by repeated signal fault tests.
- Scientific scope is conditional: synthetic B10/B11 noise controls are not physical hardware validation; simultaneous global false discovery across unlimited repeated runs is not claimed; trusted native plugins are fault contained, not sandboxed for hostile code. Linux x86-64 CI is verified, other hardware/OS targets are not.
