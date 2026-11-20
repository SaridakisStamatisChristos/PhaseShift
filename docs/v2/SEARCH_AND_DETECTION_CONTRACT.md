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
