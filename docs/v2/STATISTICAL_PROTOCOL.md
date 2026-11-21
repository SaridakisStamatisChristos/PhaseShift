# PHASESHIFT v2 — Statistical Protocol Boundaries

The **existing v1** pipeline uses independent frozen fixed-sample confirmation, exact-or-simulated paired sign flips, a 99% paired case bootstrap, and one-hypothesis BH adjustment. Cases with invalid answers, worker faults, censored statuses or insufficient independent samples are not valid timing wins.

The **new v2 Holm FWER primitive** implements a classical step-down adjustment for one predeclared fixed-size family of valid marginal p-values. Its mathematical validity is not a guarantee that exploratory selection, repeated testing, or overlapping holdouts are controlled. It has reference, edge and negative tests. It is **not yet connected to a sealed campaign registry**, so results produced by different H1 confirmations must not be described as family-wise controlled.

Multi-change and interaction scores are **discovery ranking heuristics**, not frequentist p-values. A model score, local interval or seed-matched contrast is not itself an independent confirmatory attestation. The v1 confirmation method is preserved; no optional stopping or repeated holdout reuse is authorized.

**Required before a full v2 multiple-hypothesis claim:** a versioned frozen campaign manifest, K-bound predeclaration, separate discovery and holdout seeds/cases, exactly one interpretation per sealed confirmation, Holm adjustment over all members including negative outcomes, immutable family provenance, correlated-null stress simulations and strict crash-resume handling.

## Fixed-family v2 campaign registry (incremental M7)

`freeze_campaign_v2` validates and hashes a versioned manifest **before any holdout samples**, with an immutable list of 1–16 already-frozen performance hypotheses, predeclared fixed holdout seed-pair count (20–100), test `paired_sign_flip_v1`, and family error rate `alpha` (0.001–0.25). The additive SQLite v9 schema is activated **only** by first campaign freeze; v1 evidence stays v7 and ordinary v2 remains v8.

Positive members require an existing independent `confirmation_seals` row with complete linked evidence and a one-time internally recorded, strictly positive sign-flip p-value. Negative/incomplete members contribute conservatively `p=1` and never become family-wise positives. Finalization stores a single immutable Holm adjusted result vector with every member (including nonfindings) and prevents rerun/re-freeze. No optional stopping or missing-evidence promotion.

**Scope limit:** The v2 discovery and CLI still need multi-H1 candidate routing and the p-value recorder wired into `confirm`. This commit establishes only the evidence registry, schema migration and falsification tests; it does **not** grant family-wise confidence to older unregistered confirmations. Synthetic unit p-values check arithmetic and schema policy, not real-world FWER calibration.

## CLI workflow (research only)

Run `phaseshift explore --config ... --out RUN` to finalize discovery, and freeze the family **before any** `confirm` execution:

```bash
phaseshift campaign-freeze --run RUN --manifest campaign.json
phaseshift confirm --run RUN --hypothesis H1
phaseshift campaign-finalize --run RUN --campaign C1
```

The manifest must contain `schema_version:1`, `campaign_id`, ordered `members`, `alpha`, `confirm_seed_pairs` equal to the run's frozen confirmation blocks, and `test:"paired_sign_flip_v1"`. The current campaign supports performance contrasts only. No synthetic calibration, crash/OOM/correctness or insufficient-holdout result is eligible as a positive statistical finding; negative members contribute p=1. The CLI intentionally declines to invent user-supplied p-values. `confirm` writes a p-value only after a valid positive sealed holdout (v2, existing campaign member) and uses a distinct seed cohort for H2..H16. Other v2 experiments and all v1 experiments retain their historical single-H1 behavior.

**Limitation:** A frozen campaign must contain *all* intended candidate IDs before the first confirm; multiple-candidate discovery IDs and a real-workload campaign challenge are separate gates, not asserted complete here.

Multi-change discovery now preserves up to 16 distinct frozen candidate geometries instead of recording only H1 and discarding the others. Results remain exploratory until individual independent holdouts complete and the predeclared family is Holm-finalized. The generated alternatives do not automatically enter a campaign or permit opportunistic testing/retesting without multiplicity accounting.

## Reporting distinction

For v9 campaign runs, generated summaries and reports attach `campaign_id`, `campaign_state`, `holm_adjusted_p` (only when finalized), `familywise_reject`, and `familywise_status` to participating hypotheses. Raw one-hypothesis `CONFIRMED` is **not** presented as family-wise accepted while a campaign is merely frozen or has not been registered. The raw hypothesis status is preserved for v1 compatibility; the new field is the distinct family-inference claim.


## Confirmation cohort isolation hardening

The v2 confirmed-H1..H16 seal now passes the exact immutable set of 20–100 seed-group IDs generated for that hypothesis's fixed independent holdout (distinct windows for H2..H16). SQLite's sealed evidence may not be supplied by a different candidate's held-out observations with the same boundary coordinates. Duplicate, missing or mismatched groups fail closed; oracle checks and discovery-overlap exclusion still apply. Legacy v1 seal callers without prescribed seed groups keep unchanged behavior. An adversarial test creates 20 valid but wrongly attributed pairs and requires refusal before adding the true cohort.

## Controlled null stress campaign

The deterministic `v2_correlated_null_holm_smoke` generates 3,000 independent *simulated* all-null campaigns for family sizes 1, 2, 4, 8 and 16 under independent, perfectly shared and partially correlated sign streams. It computes *exact discrete* two-sided binomial sign-test p-values under each marginal null and applies Holm step-down at alpha 0.05. The observed campaign-wise false-positive fraction is required to be <=0.075 as a **coarse Monte Carlo smoke**, not an estimate of the engine's population FWER and not a guarantee under selection or reuse of real-world holdouts. All v2 campaign claims still require generated real-worker evidence with independent seed groups and immutable outcome seals.
