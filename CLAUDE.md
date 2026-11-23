@AGENTS.md

# PhaseShift working rules for Claude Code

## Session start
When asked to "continue development" (or at the start of any session), read [`docs/HANDOFF.md`](docs/HANDOFF.md) first, run its start checklist, then execute its **Next task**. Update `docs/HANDOFF.md` and `docs/v3/IMPLEMENTATION_LEDGER.md` before ending the session. The path to a SOTA claim is planned in [`docs/v3/SOTA_ROADMAP.md`](docs/v3/SOTA_ROADMAP.md).

## Authority (highest first)
1. `docs/PHASESHIFT_SPEC.md`: immutable functional contract.
2. `PHASESHIFT_V3_SOTA_MASTER_ARCHITECTURE_DIRECTIVE.md`: V3 architecture requirements. It is a specification, not evidence of implementation.
3. `AGENTS.md` (imported above): engineering and evidence rules, including R7 verification. Its "Historical v1 handoff" section is superseded by `docs/HANDOFF.md`.
4. Source, tests, Git history and recorded CI results are the only evidence of what is implemented. Never mark a requirement complete because a document describes it.

## Development
- Core C++20 first. Do not change algorithm code unless the task explicitly asks for it.
- Preserve V1/V2 behavior, versioned encodings, seeds, case IDs, CLI defaults and policy names. New protocols get new versions.
- Prefer minimal, tested changes over rewrites. Use deterministic seeds, independent oracles and raw, replayable measurements.
- Keep errors, censored observations, failed campaigns and negative results visible. Never reinterpret a wrong answer, crash, OOM or timeout as a fast finite result.
- Never fabricate benchmarks, speedups, significance, or SOTA claims.

## Evidence and benchmarking
- Compare against genuine, correctly configured baselines (e.g. Optuna 4.6.0, SMAC3 2.4.1 adapters). Keep pinned worker semantics, legal query domains and declared budgets.
- Report CPU time, wall time, discovery quality and total computational cost.
- Label results as exploratory development evidence, independent confirmation or sealed evaluation. Freeze hypotheses before confirmation.
- The original V18 sealed synthetic cohort is already opened. Never use it for tuning or present it as a fresh holdout.
- Preserve original frozen benchmark artifacts and manifests. Never edit sealed evidence to improve results.
- Keep output datasets in ignored `runs/`. Do not commit binaries, secrets or large data blobs.
- A synthetic detector threshold is an engineered calibration, not a discovered algorithmic phenomenon.

## Verification (before any milestone or release claim)
1. Identify the exact defect or gap, inspect only the relevant code and tests.
2. Implement the smallest complete fix; add regression, edge-case and adversarial tests.
3. Build and test with CMake/CTest. For release claims also run GCC and Clang `ctest`, ASan/UBSan, and `scripts/acceptance_smoke.sh`.
4. Never weaken or remove a test to make CI pass. Fix the cause.
5. Report only what was actually run. Say when a step was skipped or failed, with output.

## Git
- Work directly on `main` (owner instruction, 2026-11-23, after v3 PR #6 was merged as `e7169dd`). Verify the branch and `git status` before changes.
- Do not force-push, rewrite history, or discard unrelated edits. Never re-run opened cohorts (7101-7120, 51001-51020) or the one-time `v18-sealed-evaluation` / `v3-external-confirmation` openings.
- Make atomic commits, each coherent and tested. Do not commit secrets.
- Push verified milestones, then check GitHub CI. Do not invent CI results.

## Communication
Keep responses short and engineering-focused. For each milestone report: changes, tests actually passed, measured performance differences, commit SHA and CI status, unresolved issues, and the next concrete step. State uncertainty plainly.
