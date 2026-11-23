# V3 architecture decisions — 21 November 2026

Source of authority: [PHASESHIFT_V3_SOTA_MASTER_ARCHITECTURE_DIRECTIVE.md](../../PHASESHIFT_V3_SOTA_MASTER_ARCHITECTURE_DIRECTIVE.md). These decisions are implementation records, not benchmark results.

## ADR-V3-001 — Preserve v1/v2 control-plane semantics during extraction
**Decision:** move the old v2 selector factory and policy-state bookkeeping to `include/phaseshift/v3/legacy_policy_adapter.hpp` while retaining existing `src/experiment.cpp` worker, Case, SQLite and confirmation ownership. No version/ID/generator default is changed.
**Why:** an all-at-once rewrite of a 169 KB experiment coordinator would be difficult to falsify. The adapter reuses v2 specialist classes and constants byte-for-byte and has a direct differential characterization CTest.
**Follow-up:** refactor the coordinator in further characterization-covered slices; check actual Case ID/schedule transcripts on the live CI worker before marking exact preservation VERIFIED.

## ADR-V3-002 — Use the existing indexed legal-space oracle for v3 slices
**Decision:** preliminary `AxisSlicePolicyV3` receives an `AdmissibleSearchSpace` reference. A region is a fixed categorical/nuisance context plus a **declared ordered** primary axis. Allowed primary values are found with `index_of`; unknown/illegal coordinates cannot be proposed. The policy never orders nominal values. The proposal list and active contexts are bounded; no virtual full-grid allocation.
**Trade-off:** an individual context initializes in `O(L * d * lookup_cost)` for L up to 4096 ordered labels, and refinement currently scans at most bounded active contexts; this is an initial reference challenger, not a proved logarithmic region-index implementation. Its scores are heuristics, not p-values.
**Falsifiers:** a disjoint conditional-space independent enumeration; deterministic replay; missing/failed metrics; 2.048B legal virtual coordinates with bounded extra RSS.

## ADR-V3-003 — Holdout separation by input contract
**Decision:** `DiscoveryEvidenceSnapshot::append_committed` and `AxisSlicePolicyV3::on_committed` explicitly reject confirmation cohorts. Frozen confirmation remains owned by the inference/Store layer; search has no direct holdout handles.
**Limitation:** an incorrectly labeled caller could still breach the boundary, so end-to-end, source-provenance and tamper checks remain mandatory before full V3-03 acceptance.

## ADR-V3-004 — Opt-in activation, no premature policy promotion
**Decision:** the v3 slice policy is not selectable through the public CLI yet; V3-03 must first provide recoverable matched-seed real-worker/pair evidence and exact legacy compatibility. No synthetic improvement or heuristic effect may justify silently replacing the validated v2 baseline. The historical v2 policy adapter is an internal extraction, not a v3 policy release.

## Current evidence state
Documentation and test presence alone are not evidence of successful execution. Exact-head CI, saturated per-resource scientific benchmarks and comparable external policies must all run before any comparative leadership assertion.
