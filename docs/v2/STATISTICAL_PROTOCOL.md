# PHASESHIFT v2 — Statistical Protocol Boundaries

The **existing v1** pipeline uses independent frozen fixed-sample confirmation, exact-or-simulated paired sign flips, a 99% paired case bootstrap, and one-hypothesis BH adjustment. Cases with invalid answers, worker faults, censored statuses or insufficient independent samples are not valid timing wins.

The **new v2 Holm FWER primitive** implements a classical step-down adjustment for one predeclared fixed-size family of valid marginal p-values. Its mathematical validity is not a guarantee that exploratory selection, repeated testing, or overlapping holdouts are controlled. It has reference, edge and negative tests. It is **not yet connected to a sealed campaign registry**, so results produced by different H1 confirmations must not be described as family-wise controlled.

Multi-change and interaction scores are **discovery ranking heuristics**, not frequentist p-values. A model score, local interval or seed-matched contrast is not itself an independent confirmatory attestation. The v1 confirmation method is preserved; no optional stopping or repeated holdout reuse is authorized.

**Required before a full v2 multiple-hypothesis claim:** a versioned frozen campaign manifest, K-bound predeclaration, separate discovery and holdout seeds/cases, exactly one interpretation per sealed confirmation, Holm adjustment over all members including negative outcomes, immutable family provenance, correlated-null stress simulations and strict crash-resume handling.
