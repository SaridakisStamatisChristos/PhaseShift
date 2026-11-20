# PHASESHIFT v1.0 — fresh independent audit remediation

**Starting source revision:** GitHub `main` `c9193249be3a6eeeb11fa0662729b28b38309d6c`.
**Audit source:** `PHASESHIFT_V1_FRESH_INDEPENDENT_AUDIT_2026-11-20.md` (2026-11-20).
**Scope:** CPU-only C++20 local Linux algorithm research engine. No GPU, macOS,
Windows, external SaaS or generic enterprise deployment requirements are added.

## Audit-findings ledger

| Finding | Remediation | Regression / trace |
| --- | --- | --- |
| FRESH-01 scaling mixed invalid statuses | Reject a complete case if any required worker observation is wrong, missing, truncated, duplicated, or nonpositive; quarantine from discovery fits and from new-size confirmation; preserve raw invalid samples | `fresh_audit_regressions`, exact poisoned three-block plugin: every invalid case is marked invalid, no fit; `scaling_independent_holdout_knee` retains positive calibration |
| FRESH-02 native plugin replay | Load the recorded algorithm ID and SHA-256 from read-only SQLite; reject changed/unavailable plugins before replay; execute via bounded native worker, validate independent oracle and output digest; do not mutate old sample evidence | `fresh_audit_regressions` verifies intact and swapped plugin handling and built-in replay |
| FRESH-03 explicit axis silently substituted | Reject an explicitly supplied primary axis not declared in `axes`; infer only for omitted primary axis | `fresh_audit_regressions` tests both cases |
| FRESH-04 comparison ignores changed native binary | Compare stored algorithm versions, ABI and output contract; fail closed on plugin mismatch even when path is identical | `fresh_audit_regressions` rebuilds the same plugin filename with different bytes |
| FRESH-05 ambiguous replay prefix | Use the existing `locate_case` helper, require unique prefix | `fresh_audit_regressions` constructs a colliding case-ID prefix |
| FRESH-06 unordered categorical minimization | Pair-preserving nuisance-structure shrink; revalidate BOTH frozen unordered labels with independent paired cohorts; preserve all candidate-level raw samples, trace and non-successful shrink results. Never assign a scalar distance | `fresh_audit_regressions` validates 20-cohort categorical witness and no false minimization claim |
| FRESH-07 ignored report format | Explicit supported format validation and selective output refresh for markdown/json/csv/html; default preserves full export | `fresh_audit_regressions` validates unsupported format rejection and one-format output behavior |
| FRESH-08 negative finding marked attested | Only sealed positive confirmations set `confirmation_evidence_attested=true`; add explicit applicability boolean | `fresh_audit_regressions` checks negative finding and prior positive seal integrity tests |
| FRESH-09 stale merge note | Append post-merge provenance note to historical Trillion v1 repair ledger | `docs/v1_audit_closure_trillion.md` |

## Scientific safety boundaries

A case with even one wrong answer is ineligible for performance ranking and
regime fitting. Raw failed observations remain available in SQLite and CSV.
A native plugin path is not a version identity: compare and replay must check
the persisted binary digest. Replay is not a security sandbox for malicious
untrusted native code. Categorical indices identify labels, not numeric
coordinates; absence of structural shrink is reported without claiming
minimality. The independent holdout decision remains separate from
exploratory growth-model fitting. None of these tests proves a general
asymptotic law, causal discovery or universal timing portability.

## Verification and publishing

Run `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2`,
`ctest --test-dir build --output-on-failure`, and `bash scripts/coverage.sh`.
Run the ASan/UBSan and Clang CI matrices for the exact published commit before
claiming remote qualification. This ledger records implementation scope and
reproducer requirements, not an unobserved remote-CI result.
