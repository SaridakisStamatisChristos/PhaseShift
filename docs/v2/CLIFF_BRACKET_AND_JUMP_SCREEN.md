# PhaseShift v2: opt-in cliff bracketing and persistent-jump diagnostic

**Research state: exploratory. Not a statistically validated superiority or a SOTA claim.**

## Compatible versions

No default v1 or existing v2 search policy changes. The new opt-in policy
`search.method: "cliff_bracket_v2"` with `search.policy_version: "v2"`
uses the same bounded, deterministic `HierarchicalPolicyV2` mechanism, but
prioritizes large measured contrasts within an ordered numerical axis before
gap width. It requires a single numeric primary axis, one replicate, and
no admissible-subspace restrictions. The old `hierarchical_boundary_v2`
priority and replay transcript are unchanged.

A second opt-in `detection.detector_version: "multi_change_jump_v2"`
screens changepoint cuts with **three measured bins on each side**. It requires
a same-direction immediate step, a persistent median shift above the
predeclared practical log-ratio, and edge contrast exceeding 3x the upper
quartile of other adjacent changes. It rejects cuts near the endpoints
rather than inventing unsupported persistence. DP segmentation candidates
remain exploratory until independent confirmation.

## Small config fragments

```json
{
  "search": {
    "method": "cliff_bracket_v2",
    "policy_version": "v2",
    "max_cases": 8,
    "exploration_fraction": 0.0
  },
  "detection": {
    "detector_version": "multi_change_jump_v2",
    "min_practical_ratio": 1.2
  }
}
```

These fragments must be included inside a valid full experiment config.
The conservative controls can reject real but weak/noisy jumps; they are
not calibrated false-positive rates. Hard threshold results are synthetic
operation counts and cannot establish improvement on real algorithms.
Run full fair CPU/wall budget comparisons, fresh held-out data, and independent
external search baselines before recommending a production policy switch.

## Reproducible verification

- `v2_cliff_bracketing_full_threshold_oracle`: all 63 monotone thresholds
  at 64 positions, eight selected cases, and legacy policy replay parity.
- `v2_cliff_bracketing_cli_holdout_contract`: actual CLI, workers, SQLite
  decision order, threshold localization, and negative control.
- `v2_noisy_holdout_segmentation_challenge`: 20-seed noisy cliff, null,
  smooth-trend, and impulse tests of the optional persistence screen.

Each is a correctness/diagnostic gate, *not* proof of head-to-head speedup.
