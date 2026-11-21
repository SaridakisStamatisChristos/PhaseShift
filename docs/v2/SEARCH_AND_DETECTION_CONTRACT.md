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
