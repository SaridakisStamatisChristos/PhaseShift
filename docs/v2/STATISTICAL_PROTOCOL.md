# PHASESHIFT v2 — Statistical Protocol Boundaries

The **existing v1** pipeline uses independent frozen fixed-sample confirmation, exact-or-simulated paired sign flips, a 99% paired case bootstrap, and one-hypothesis BH adjustment. Cases with invalid answers, worker faults, censored statuses or insufficient independent samples are not valid timing wins.

The **new v2 Holm FWER primitive** implements a classical step-down adjustment for one predeclared fixed-size family of valid marginal p-values. Its mathematical validity is not a guarantee that exploratory selection, repeated testing, or overlapping holdouts are controlled. It has reference, edge and negative tests. It is **not yet connected to a sealed campaign registry**, so results produced by different H1 confirmations must not be described as family-wise controlled.

Multi-change and interaction scores are **discovery ranking heuristics**, not frequentist p-values. A model score, local interval or seed-matched contrast is not itself an independent confirmatory attestation. The v1 confirmation method is preserved; no optional stopping or repeated holdout reuse is authorized.

**Required before a full v2 multiple-hypothesis claim:** a versioned frozen campaign manifest, K-bound predeclaration, separate discovery and holdout seeds/cases, exactly one interpretation per sealed confirmation, Holm adjustment over all members including negative outcomes, immutable family provenance, correlated-null stress simulations and strict crash-resume handling.

## Fixed-family v2 campaign registry (incremental M7)

`freeze_campaign_v2` validates and hashes a versioned manifest **before any holdout samples**, with an immutable list of 1–16 already-frozen performance hypotheses, predeclared fixed holdout seed-pair count (20–100), test `paired_sign_flip_v1`, and family error rate `alpha` (0.001–0.25). The additive SQLite v9 schema is activated **only** by first campaign freeze; v1 evidence stays v7 and ordinary v2 remains v8.

Positive members require an existing independent `confirmation_seals` row with complete linked evidence and a one-time internally recorded, strictly positive sign-flip p-value. Negative/incomplete members contribute conservatively `p=1` and never become family-wise positives. Finalization stores a single immutable Holm adjusted result vector with every member (including nonfindings) and prevents rerun/re-freeze. No optional stopping or missing-evidence promotion.

**Scope limit:** The v2 discovery and CLI still need multi-H1 candidate routing and the p-value recorder wired into `confirm`. This commit establishes only the evidence registry, schema migration and falsification tests; it does **not** grant family-wise confidence to older unregistered confirmations. Synthetic unit p-values check arithmetic and schema policy, not real-world FWER calibration.
