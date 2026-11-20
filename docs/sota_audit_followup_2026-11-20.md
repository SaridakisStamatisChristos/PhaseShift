# PHASESHIFT SOTA audit repair — independent counterexample closure

Source audit: `PHASESHIFT_SOTA_SPEC_ALIGNMENT_AUDIT_2026-11-20.md`, baseline GitHub `main` `7f46fead9207db98e97210bb6e9f3fa78895614d`.

| Finding | Implementation | Regression | Verification state |
|---|---|---|---|
| F-NEW-01 crash on null algorithms / numeric `fixed` | `src/experiment.cpp::validate_configuration_shape` checks schema types before JSON conversions | `tests/run_sota_audit_falsifiers.cmake.in` checks both null algorithms, wrong object and null root | Local execution verified; GitHub CI not yet verified |
| F-NEW-03 ignored/converted config options | Key allowlists and typed integer/real/string/object checks on root, search, detection, limits, measurement, and axes | 16 malformed negative cases with exit code 2, plus all shipped configs valid | Local execution verified; GitHub CI not yet verified |
| F-NEW-02 global wall budget overshoot | A monotonic absolute deadline is passed to `measure`, IPC and worker lifecycle; budget expiry aborts in-flight case, records `wall_budget_exhausted`, emits report, exits nonzero. Partial prior decisions remain resumable | Valid large sorting `n=20000`, 9 blocks, 1s budget that previously took 10.62s and exited zero | Local execution verified at ~1.02s; GitHub CI not yet verified |
| F-NEW-04 virtual-memory vs RSS misnomer | Legacy setting retained for compatibility; parent samples worker VmRSS around 25ms throughout response wait, release retains distinct RLIMIT_AS; memory status reported as out_of_memory when sample crosses bound | Existing worker fault/OOM suite and sanitizer CLI; no proof of continuous instantaneous cap | PARTIAL: sampled guard implemented; physical-spike testing and exact peak RSS guarantee not proven |
| F-NEW-06 scorecard misleading limit note | `src/experiment.cpp` conditional explanation consistent with measured witness-reduction percentages versus NA | Existing ablation scorecard tests | Local source verified; GitHub CI not yet verified |
| F-NEW-05 physical noise/generalization | Explicitly retained as a **research evidence gap**, not represented as a solved software defect | B10/B11 deterministic simulations remain calibration only | UNVERIFIED for actual thermal/cache/frequency drift |

## R7 acceptance constraints

- The independent malformed-config reproducer must exit 2 with a diagnostic, never a signal. No silent acceptance of unknown settings.
- The valid heavy sorting workload with one-second budget must return nonzero within documented scheduling cleanup tolerance, preserve a SQLite report, and never count an unfinished case as successful.
- All 32 registered CTests, GCC/Clang Release, ASan/UBSan and natural CLI smoke require actual execution evidence on the modified revision before a full local/remote PASS label.
- No assertion of general statistical validity under unmeasured hardware noise, exact instantaneous RSS enforcement, or cross-platform certification.

**No automated release, version tag, or new project license is authorized by this repair.**
