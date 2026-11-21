> **Status notice (R7 continuation, 21 November 2026):** This file contains historical checkpoint observations; consult [CONTINUATION_STATUS.md](CONTINUATION_STATUS.md) for the current exact-revision evidence and open hard gates. Earlier “pending latest CI” statements describe older revisions, not the current branch head.

# PHASESHIFT v2 — Search and Detection Contract (experimental slice)

## Semantics and compatibility
- v1 config defaults, case identities, worker validation, plugin ABI, SQLite schema v7, and fixed-sample confirmation remain baseline behavior. v2 is explicitly opt-in through `search.policy_version=v2`.
- Valid v2 methods currently: `uniform_v2`, `hierarchical_boundary_v2`. The latter requires one strictly increasing numeric axis and one discovery replicate.
- v2 `search.replicates` accepts 1–32 **only for uniform_v2**. It gives full distinct case identities at `(coordinate, replicate_seed)` and a deterministic seeded virtual proposal permutation. Uniform over coordinate×replica pairs does not guarantee every coordinate receives every seed within a partial budget.
- Indexed universe: axis list storage O(sum axis sizes), mixed-radix ranking O(number of axes), checked cardinality <= signed SQLite index bound. Sparse exact no-replacement permutation stores O(number drawn) entries. Rejection of invalid coupled axes is bounded at 10,000 attempts and can abstain; it is NOT a general uniform sampler over valid conditional subspaces.
- Hierarchical policy keeps ordered observations in O(k) memory, uses neighboring interval priority with difference/uncertainty heuristics and observed worker cost. Exact resume derives selection from persisted samples and indexed deterministic PRNG state. The heuristic score is not a calibrated posterior.
- A complete case must pass the independent oracle. Crash/timeout/OOM/wrong-answer is a status, never a timing win. Signed/censored statuses retain original v1 handling.
- Paired common replicate seed holds across structural coordinates where both coordinate/seed pairs are sampled; this does not imply identical nuisance morphology or causal attribution.

## Detectors
- `boundary_v1` default unchanged. The opt-in `detection.detector_version=multi_change_v2` uses piecewise constant K-bounded dynamic programming on valid finite-range discovery metrics, with minimum 2 observed coordinates per segment, up to 3 changes and an explicitly heuristic penalty.
- DP worst-case O(KN²) time and O(KN) memory; N<=4096 and K<=32 at the primitive boundary. Long-double centered sums mitigate cancellation. PELT, slope-change regression, broad smooth/no-boundary taxonomy and calibrated localization coverage are NOT implemented.
- At runtime, multiple discovered changes become ranked **discovery-only events**. One best candidate can become H1 for existing frozen single-hypothesis independent holdout; no campaign family-wide multiplicity guarantee is claimed.
- Two-way 2×2 nominal interactions require complete exact-seed cohorts, reject duplicate/missing/nonfinite cells, and distinguish conditional status from log-performance contrasts. The current live v2 path emits an exploratory event only, never `CONFIRMED` solely from interaction screening.

## Unmet requirements
Adaptive discovery replication, weighted/conditional search, multidimensional hybrid policy, uncertainty calibration, PELT, complete optional-stopping theory, accurate causal labels, full v2 persistence/migration, and competitive SOTA status remain outstanding.


## Exact indexed admissible subspaces (M2 incremental)

Opt-in `search.admissible_subspaces` accepts 1–256 explicitly disjoint rectangles of allowed axis labels. Each rectangle may omit unrestricted axes. Overlapping, empty, duplicate-label, unknown-axis, and off-domain rectangles fail closed. The valid union has a checked exact cardinality and mixed-radix random access; `uniform_v2` samples uniformly without replacement across the declared union and replicate indices, with no product enumeration. Resident storage is O(B × total axis label storage); index access O(log B + number of axes), overlap checks quadratic in B. The declaration is not a proof that all generated cases satisfy independent generator coupling constraints: nonrectangular constraints and invalid generator combinations are still outside this slice and retain existing bounded rejection. v1 does not accept this opt-in field. The hierarchical method does not yet support conditional subspaces.


## Stratified primary-axis exploration (M3 incremental)

Opt-in `search.method=stratified_v2` uses one exactly permuted without-replacement sampler **per primary-axis stratum**, interleaved round-robin in a seeded random stratum order. Each sampler maps its local index to the original admissible union and replicate index using mixed-radix arithmetic (no full product generation). Every nonempty primary stratum is reached before revisiting a stratum; all candidates remain exactly-once over a full sweep. This is intentionally **not uniform** across unequal-size strata. CPU costs, calibrated uncertainty, adaptivity, and held-out superiority are not implied. The method is an internal baseline that can expose rare primary-axis regions under small budgets. v1 and hierarchical schedules are unchanged.


## Pruned exact changepoint model (M5 incremental)

Opt-in `detection.detector_version=multi_change_pelt_v2` supports squared-error penalized segmentation with delayed PELT candidate pruning under a minimum segment size. An unconstrained optimal partition containing more changes than the configured K-bound is **not truncated**: it falls back to the existing exact K-bounded DP. Worst-case complexity remains quadratic for the pruned search and O(KN²) for fallback. Unit tests compare every result to the exact K-bounded oracle on exhaustive binary sequences and test zero/smooth/multiple transitions. Scores are exploratory; candidate selection never establishes independent holdout significance.


## Ranked candidate identity and confirmation multiplicity

Opt-in `multi_change_v2` / `multi_change_pelt_v2` now persists at most 16 distinct geometric hypotheses (`H1..H16`) at the point discovery is complete; it keeps the strongest selected `H1` unchanged and stores ranked alternatives as **HYPOTHESIZED** only. Repeated identical left/right geometry is deduplicated rather than claiming distinct conditional causal scopes from the same axis cut. These are not 16 confirmed findings. Each hypothesis has a separate deterministic, nonoverlapping fixed-holdout seed window. The campaign must be frozen **before** the first confirmation to make any family-wise claim. No claim is made that every alternative can be independently confirmed under a finite run's budget.


## Weighted domain priors and per-case cost provenance (2026-11-21 continuation)

A new **opt-in** `search.method=weighted_v2` accepts `search.domain_priors` maps of axis label to finite positive numerical weight (inclusive 1e-6..1e6). Missing labels/axes carry unit prior; unknown labels, zero/negative/nonfinite weights, malformed maps and use by v1 or another v2 method are rejected. Rectangular admissible subspaces must be pairwise disjoint (existing union contract). Candidate selection is **probability proportional to the product of declared axis priors among remaining valid coordinates and equally weighted replicate seeds**. It uses a sparse removed-prefix mass ledger, direct virtual indexing, and deterministic PRNG replay; neither full candidate materialization nor rejection-based biased fallback is used. The floating-point numeric-resolution limit fails closed rather than pretending to provide exact probabilities below machine resolution.

Memory is O(sum normalized label maps across declared rectangles + emitted candidates × axes); a single weighted proposal scans per-axis declared labels and rectangular blocks, not their Cartesian product. This is an admissibility-constrained exploration **policy** rather than calibrated uncertainty or a SOTA search result. A large virtual universe is not a large completed campaign.

`v2_phase_budget` is persisted inside the same decision/observation transaction and records per-case scheduler-proposal wall ns, worker-invocation wall ns, valid timed-region sample CPU ns and a Boolean completeness flag. These descriptive wall clocks are not an enforceable equal **parent/process-tree CPU** budget. The contract requires callers to keep source/confirmation evidence separate and not claim fairness from these fields alone.

Natural example: `configs/v2/weighted_admissible.json`; executable gates: `weighted_v2_no_replacement_oracle`, `weighted_v2_resume_admissible`, `weighted_v2_invalid_config_contract`. See `tests/verify_v2_weighted.py` for independent SQLite replay and phase-event linkage. The new code remains experimental pending exact-revision CI; all old v1 policy/case identity rules remain intact.
