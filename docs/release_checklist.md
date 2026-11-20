# R7 verification and acceptance ledger

The following labels describe *implemented paths*, not a claim that every target/platform has been verified. Exact run results belong in `docs/evidence_ledger.md` and local CI outputs.

| Gate | State | Evidence / gap |
|---|---|---|
| G0 Repository baseline | PASS locally | empty target repository confirmed through GitHub; CPU-only toolchain inventoried |
| G1 Reference algorithms | PARTIAL | differential randomized/edge tests for arrays, Top-K, graphs; broader thousand-case adversarial campaigns pending |
| G2 Harness isolation | PARTIAL | child worker, bounds, correctness-before-timing, fault statuses; no persistent worker, no extensive fault-injection matrix |
| G3 Boundary inference | PARTIAL | deterministic synthetic positive and null tests, exact threshold; single-hypothesis case-level 99% bootstrap available; no family-wise multi-hypothesis calibration or 2D inference |
| G4 Replay/minimization | PARTIAL | input SHA/regeneration/replay, synthetic minimal witness; family-aware ddmin pending |
| G5 Cross-experiment comparison | PARTIAL | raw samples and distinct environment records; synthetic equal-budget search ablation available; fair real-workload search comparison incomplete |
| G6 Release hardening | PARTIAL | local Release and Debug+ASan/UBSan CTest; CI outcome and non-x86-64 builds not established |

**Not v1.0:** no resumable scheduler, scaling fit suite, all requested plugin fault cases, or full schema migration chain. No unsupported claim of production, research-grade, novel, or SOTA performance.
