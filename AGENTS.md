# R7 continuation and evidence rules

The immutable functional contract is `docs/PHASESHIFT_SPEC.md`. R7 and any contributor must preserve independent oracles and deterministic case/replay identity, version the generator when serialization changes, keep raw measurements and failure status unmodified, and never reinterpret a wrong answer, OOM or timeout as a finite fast result.

Implement and test the C++20 core first. Treat native plugin process isolation as fault containment rather than a secure sandbox; use the trusted v1 ABI. Verify `cmake --build`, GCC/Clang `ctest`, ASan/UBSan, and `scripts/acceptance_smoke.sh` before a release claim. Numeric models are empirical. Freeze hypotheses before independent confirmation; preserve fresh-seed paired-case 99% evidence, multiplicity policy and negative or contradictory outcomes. Prioritize unbiased verification over a positive result.

Each Git commit must contain a coherent change, reproduce relevant tests and preserve unrelated user edits. Use a separate development branch and draft PR; do not force push, merge, rewrite history or invent CI evidence. Keep output datasets in ignored `runs/` and avoid committing binaries, secrets or massive data blobs. A synthetic detector threshold is an engineered calibration, **not** a discovered algorithmic phenomenon.


## Historical v1 handoff (2026-11-20; superseded by `docs/HANDOFF.md`)

Core v1 source lives on `r7/v1-acceptance-hardening`. All 21 distinct tests have passed GCC Release, Clang Release and ASan/UBSan locally; `bash scripts/acceptance_smoke.sh` is a further natural-command acceptance check. A full CI runner outcome is still needed for a GitHub-hosted verification claim. The `reproduce` CLI must remain standard-library-only for built-in algorithms, and no plugin source may be fabricated. Keep undirected graph reciprocal arcs paired through literal ddmin. The final release checklist and precise pass evidence are in `docs/release_checklist.md` and `docs/evidence_ledger.md`. Do not silently infer global statistical error control, hardware-independent timings or secure execution of untrusted plugins.
