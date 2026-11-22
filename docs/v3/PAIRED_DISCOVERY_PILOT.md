# PhaseShift v3 — matched-seed real-worker discovery pilot

**Status:** Experimental V3-03 **PARTIAL**, not wired into main `explore`, not a statistically confirmed boundary or SOTA finding.

This opt-in C++20 executable connects the existing deterministic `ReplicatePlanner` to real isolated workers and a separate authoritative `run.sqlite` using schema v10. It uses a validated v2 configuration as an immutable workload input, **without changing v2 execution, CLI or persistence**. The pilot accepts one structural axis, two legal fixed endpoints and at most 100 pairs. It does not yet choose which coordinates to explore.

```sh
./build/phaseshift_v3_pairs run --config configs/v2/uniform_replicates.json \
  --out /tmp/v3-pair-pilot --left 0 --right 3 --pairs 3 --pause-after 1
./build/phaseshift_v3_pairs verify --run /tmp/v3-pair-pilot
./build/phaseshift_v3_pairs resume --run /tmp/v3-pair-pilot
```

**Replay contract:** Frozen config bytes/hash, seed, execution order, pair identity, input bytes/digest, raw sample statuses/timings and per-pair cost are stored. Resume reconstructs the fresh seeded proposal schedule and input bytes, rejecting inconsistent samples instead of silently repairing immutable source evidence. A pair is committed atomically after **both** workers finish; interruption before commit re-executes the entire pair; interruption after commit replays it. The regression gate forces both crash points.

**Valid evidence:** Each worker uses the existing independent correctness oracle. Only all-`ok` worker results produce a descriptive log contrast; failures are censored and remain present. Contrasts are **not** statistical p-values. The v3 discovery seed planner excludes a predeclared v2-style holdout seed window, but full frozen campaign holdout registry enforcement is **not yet implemented**.

**Cost scope:** `worker_timed_cpu_ns` is timed algorithm-region CPU, not complete waited-child/process-tree CPU; `pair_wall_ns` is elapsed worker block wall but excludes planning and persistence. Killed worker work performed before an uncommitted pair cannot be fully accounted for. Full resource-budget gating and cancellation are **OPEN**.

**Remaining V3-03 requirements:** Adaptive replicate-versus-new-coordinate decisions; generalized conditional/legal structural pair selection; independent campaign confirmation seed registry; full CPU and wall accounting; lost-work crash ledger; robust dispersion and uncertainty decisions; external matched comparisons.


## Optional v11 adaptive matched discovery

The previous fixed-endpoint runner stays on **schema v10**, unchanged. The new optional v11 mode selects between new adjacent legal ordered-axis pairs and fresh matched-seed replications of ambiguous observed contrasts.

    ./build/phaseshift_v3_pairs adaptive --config configs/v2/uniform_replicates.json --out /tmp/v3-adaptive-pilot --pairs 6 --pause-after 2
    ./build/phaseshift_v3_pairs verify --run /tmp/v3-adaptive-pilot
    ./build/phaseshift_v3_pairs resume --run /tmp/v3-adaptive-pilot

**Frozen allocation:** The experimental adaptive-pair-policy.1 enumerates adjacent legal numeric-axis structures using a sparse without-replacement permutation. The first two pairs explore; every fourth later pair reserves global exploration if available. Other turns use fresh matched seeds to replicate a legal prior pair ranked by distance to a frozen practical effect and bounded independent-seed MAD heuristic. Failed/censored pairs are never eligible for a finite-effect replication. This is ranking only, not a posterior, p-value or calibrated interval.

**Persistence:** Separate opt-in schema v11 adds v3_adaptive_decisions to bind pair number, axis endpoints, explore/replicate arm and SHA-256 of the predecision committed-only evidence state. Pair decision, both raw workers and case input bytes are committed atomically. Verify/resume rebuilds each action and numeric effect from frozen source and SQLite, rejecting tampering. No confirmation handle reaches this policy.

**Scope limits:** One numeric ordered axis, 3–4096 declared labels and at most 100 matched discovery pairs. Full campaign holdout registry, adaptive multidimensional conditional cells, calibrated uncertainty, complete spent-cost accounting after killed uncommitted workers, statistical multiplicity and external SOTA evidence remain unimplemented. Paired versus mismatched-seed validation is a controlled synthetic oracle, not evidence that real generators are fully exchangeable.

## Optional v12 multi-axis conditional paired discovery

V12 is separate from the v10 fixed mode and v11 one-axis adaptive mode. It creates a **new** SQLite v12 run database and leaves v1/v2 and earlier pilots unchanged. Example using an existing disjoint-rectangle config:

    ./build/phaseshift_v3_pairs conditional --config configs/v2/admissible_calibration.json --out /tmp/v3-conditional --pairs 6 --pause-after 2
    ./build/phaseshift_v3_pairs verify --run /tmp/v3-conditional
    ./build/phaseshift_v3_pairs resume --run /tmp/v3-conditional

Every pair differs in only one primary numeric axis and retains the same categorical/non-primary context and fresh seed. The policy operates in the actual legally indexed disjoint union without a Cartesian allocation. Its deterministic discovery budget first screens declared legal rectangular strata, then trades global new-context/edge screening against uncertainty-ranked replication of observed edges. For bounded cost it retains <=128 active contexts, <=4096 primary labels and <=100 pairs per run. These are **heuristics** and cannot establish a p-value or SOTA result.

Full ordered coordinate vectors, chosen action, and prior committed discovery digest are inserted in the same SQLite transaction as the two raw worker batches. Verify/restart independently recreates the schedule and observes only discovery data. A typed implication-rule config is **explicitly rejected** by this version: disjoint rectangles are supported, not arbitrary logical implications. This intentional limitation is a hard gate before any claim of general mixed conditional legality.

Remaining hard gates: campaign-wide sealed independent confirmation, full resource accounting across killed uncommitted attempts, richer conditional-rule legality and region-tree refinement, multi-family held-out policy comparison, multiplicity control and external competition.

## Optional v13 compiled logical implication rules

Existing v12 `conditional` **continues to reject** implication-rule configurations, preserving its recorded design and replay semantics. The separate v13 protocol uses the original, validated v2 `compile_conditional_rules_v2()` to transform frozen conjunctions of `WHEN axis in labels → REQUIRE axis in labels` into a bounded, pairwise-disjoint admissible rectangle union. It never enumerates the Cartesian product.

    ./build/phaseshift_v3_pairs conditional-rules --config configs/v2/conditional_implications.json --out /tmp/v3-implication-pilot --pairs 6 --pause-after 2
    ./build/phaseshift_v3_pairs verify --run /tmp/v3-implication-pilot
    ./build/phaseshift_v3_pairs resume --run /tmp/v3-implication-pilot

The new v13 SQLite user_version is part of the same creation transaction as the frozen plan. Both discovery scheduling and fresh-seed worker execution use the **compiled legal union**, not the pre-rule universe. All subsequent verify/resume operations reconstruct the same union from the original frozen configuration and use the same independent raw-record and choice re-evaluation as v12. The older v10/v11/v12 pilot schemas remain valid and separate. Typed rules with invalid axes, labels, decomposition beyond 256 rectangles, or no feasible candidates are rejected by the canonical compiler.

An independently authored tiny-domain truth oracle checks all 16 potential three-axis tuples in the calibration rule fixture, finding **six legal tuples** across three rare/common contexts. The adversarial real-worker test requires no forbidden paired endpoints, matching categorical contexts and seeds, deterministic pause/resume, pre/post-commit fault recovery and rejection of corrupted decisions/effects. All statistical and benchmark promotion claims remain unverified.

**Known gaps:** This is a rules-to-rectangle compiler, not a full multidimensional adaptive region tree. It still requires a numeric primary axis with at least two legal neighbors in a candidate context, bounded context/cardinality and no full campaign confirmation or lost-worker work ledger. Entire V3-04 remains PARTIAL pending measured held-out performance.

## Opt-in v14 conditional region-tree frontier

Version 14 adds `phaseshift_v3_pairs conditional-tree` as a separate **new-run** protocol, preserving the existing v10 fixed, v11 adaptive, v12 disjoint and v13 implication-mode replay identities. It accepts validated v2 configurations with either disjoint admissible rectangles or frozen logical implications (precompiled by the canonical v2 configuration loader).

    ./build/phaseshift_v3_pairs conditional-tree --config configs/v2/admissible_calibration.json --out /tmp/v3-tree --pairs 6 --pause-after 2
    ./build/phaseshift_v3_pairs verify --run /tmp/v3-tree
    ./build/phaseshift_v3_pairs resume --run /tmp/v3-tree

**Algorithm:** each admitted legal non-primary context owns an interval frontier over adjacent *legal* primary-axis edge positions. The portfolio screens declared disjoint rare strata before local interval splits, reserves every fourth pair for global discovery, then selects the widest legal live interval midpoint and bisects only that context. Issued edges cannot repeat during exploration; fully exhausted frontiers may use fresh matched-seed replications chosen from committed discovery-only observations. Invalid/censored evidence cannot become a finite speed metric. Memory is bounded by the existing 128-context/4096-label/100-pair caps and sparse global sampling, not the virtual Cartesian universe.

The v14 immutable predecision digest covers the conditional context map, issued edge keys, ordered live-region bounds/depth, and prior committed observations. Each proposal, pair's full coordinates, two raw worker outcomes and digests are transactionally stored in a separate new schema v14; verify/resume deterministically reconstructs each step and rejects raw/result/schedule tampering. Existing v12 and v13 protocols retain byte-identical policy paths because region mode defaults off.

**Falsification:** standalone exhaustive 2-context 32-point oracle with two opposing cliffs in categorical XOR (all 62 possible edges screened once with no duplicate), discovery-only holdout rejection, missing/nonfinite result rejection, and 2.048-billion virtual candidate RSS/regime-capped legal test. Real-worker v14 CTest checks pause/resume, both crash points, rare legal context completeness, corrupted transcript detection, v12 implication rejection and v14 valid implication support.

**Limitations:** the frontier uses deterministic midpoint partitions and a simple fixed portfolio, not an empirically proven acquisition optimum. No validated campaign-wide holdout performance comparison or resource-accounting closure; v3 remains opt-in and V3-04 remains PARTIAL under the master directive.

## Optional v15 auditable cost acquisition

    ./build/phaseshift_v3_pairs acquisition-v3 --config configs/v2/admissible_calibration.json --out /tmp/v3-cost --pairs 6 --pause-after 2
    ./build/phaseshift_v3_pairs verify --run /tmp/v3-cost
    ./build/phaseshift_v3_pairs resume --run /tmp/v3-cost

A six-pair run deliberately remains in cost-fallback mode: a sufficiently large bounded committed discovery history is needed before cost weighting is enabled. The empirical lower-median and maximum predictions refer to actual pair measurement-wall and timed algorithm-region CPU only, not full end-to-end process CPU. Every fourth pair's costs are held out from fitting and checked against the previous empirical wall envelope; this is prequential evidence on the adaptive discovery stream, not an independent frozen campaign holdout, not a confidence interval. The scored experiment records bounded dimensionless components and exact IEEE bit patterns, fitted/heldout counts, coverage and failure diagnostics. CPU is independently reconstructed from raw worker samples on replay; cost duplicates are consistency checks, not cryptographic authenticity. Fixed rare/global proposals and regular replication are not suppressed by cost. Cost weighting is disabled if cold, undercovered or censored. v10–v14 remain available without the new policy. No calibrated predictive probability or empirical superiority is claimed; the multi-family development comparison and full process/attempt accounting are separate gates.


## V3-07 read-only taxonomy

    ./build/phaseshift_v3_pairs taxonomy --run /tmp/v3-cost

For v12–v15 conditional runs this command reconstructs the exact committed discovery prefix in a read-only SQLite snapshot, including verifying the existing scheduler/worker transcript. JSON contains explicit discovery-only candidate kinds, exact categorical contexts, descriptive bounds, minimum two-pair support, rival scores and uncertainty/abstention diagnostics. Consecutive declared numeric edges with valid replicated matched measurements are integrated into relative profiles (no inferred absolute level, no gap filling); incomplete chains stay inconclusive. Crossovers require repeated matched within-seed sign reversal of paired A/B log ratios. Status frontiers carry no speed effect. Conditional interactions compare different contexts' matched edge summaries and are exploratory, not full-factorial or significance claims. The penalty is engineering scale, not probability or confidence. `confirmed` is always false. Reading results does not alter the run or any old schema. Scaling and externally calibrated localization, heteroscedastic modelling and sealed confirmation remain separate gates.
