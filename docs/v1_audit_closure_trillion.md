# PHASESHIFT v1.0 — Trillion Developer++ independent audit-finding closure

**Contract:** `docs/PHASESHIFT_SPEC.md`, especially §7.3, §8.1–8.2 and §9.4.
**Baseline:** GitHub `main` commit `dc67b066507e91b57d903382a9617cb5c976591a`.
**Scope:** local Linux x86-64 C++20 implementation, no cross-platform migration, no proprietary/cloud features.
**Version-control authority:** the source in this delivery is a **local repair overlay**, not a new verified GitHub revision or release. The repository has not been pushed by this audit closure.

| Finding | Root cause | Repaired code | Independent evidence |
|---|---|---|---|
| F-V1-01 categorical primary unsupported | Discovery returned early on nonnumeric axes; confirmation and SQLite seals assumed numerical axis values | `src/experiment.cpp` enumerates observed unordered category contrasts and exact status boundaries; `src/store.cpp` validates persisted string/bool category labels against the frozen design before confirmation sealing; report preserves category labels | `full_v1_contract`: positive work-count category confirmation, categorical wrong-answer confirmation, string-category seal, unknown-label rejection; 20 new matched seed pairs each |
| F-V1-02 stratified random silently aliases grid | Numeric strata were sequential, categorical strata ignored; categorical adaptive feedback fell back to grid | `scheduled()` uses seeded in-stratum and stratum-order shuffles, balanced interleaving, exact categorical labels; `adaptive_next()` handles categorical scope/edge coverage with recorded seeded exploration quota | `full_v1_contract`: same-seed replay, different-seed order, first-cycle coverage of all categories, numeric quantile coverage and categorical adaptive resume/identical transcript |
| F-V1-03 absolute wall deadline leakage | Several nested worker calls used only local per-sample timeouts | Shared monotonic deadline in `confirm`; worker deadline forwarded for confirmation holdout/control, intervention and all three witness-minimization predicates | `full_v1_contract`: deliberately slow child exceeds a 1-second budget and is killed before its longer sample timeout; confirmation becomes `NOT_CONFIRMED`, intervention retains partial SQLite/JSON and fails nonzero |
| F-V1-04 unused distant negative-control result | Observation was persisted but invalid oracle/worker status did not influence verdict | Predeclared control validity veto in performance and exact failure confirmation; fresh seed, independent from 20 paired cohorts, explicit fallback when design has no third point | `full_v1_contract`: valid calibration control and confirmation; plugin that works on primary pair but fails on distant control must produce `NOT_CONFIRMED` and explain `oracle_worker_valid=false` |

## Invariants

- Categorical indices are design identifiers, never continuous distances or modeled slopes. Exact labels live in the immutable run configuration and are rendered in reports.
- Every `CONFIRMED` status must pass the existing SQLite v7 independent evidence seal; category indices are resolved from persisted case parameters and the frozen category design.
- Discovery/holdout case IDs and seed groups remain independent; the additional control is **never** counted as a 21st paired seed or used to inflate the effect estimate.
- A worker error, invalid oracle, or total wall expiry cannot be turned into a favorable timing ratio. Partial-run artifacts remain inspectable.
- One frozen selected hypothesis per experiment remains the multiplicity boundary; multi-study/global error control, hostile plugin sandboxing, portability and hardware-independent performance claims are **not** implied.
- Numerical minimization remains for numeric boundaries only; unordered category contrasts are preserved as replayable evidence pairs without fake scalar minimization.

## Test commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
bash scripts/acceptance_smoke.sh
```

Additional new CTest: `full_v1_contract`. The old `categorical_axis_semantics` acceptance test was updated to allow correctly classified categorical hypotheses, without accepting a fabricated numeric changepoint. `synthetic_ablation` now asserts 65 persisted cases for a 24-case discovery + 40 cases in the 20-pair holdout + **one** independent validity-control case (previously 64), still requiring the exact engineered 23–24 boundary with zero localization error.

## Limits and evidence status

This patch's 33-test suite must pass independently in GCC Release, Clang Release, and an ASan/UBSan configuration, plus CLI acceptance; report only gates actually run. Successful tests verify the intended behaviors for tested cases—not SOTA algorithmic performance, scientific novelty, universal CPU-noise invariance, arbitrary untrusted plugins, or optional Windows/macOS support. No new SQLite migration is needed; the category references use the existing frozen configuration and v7 case/sample links.
