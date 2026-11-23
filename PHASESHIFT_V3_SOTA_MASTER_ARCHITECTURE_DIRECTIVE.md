# PHASESHIFT v3 — SOTA-Competitive Algorithmic Boundary Discovery
## Master Architecture, Research Program, Autonomous Implementation Directive, and Acceptance Contract

**Document ID:** `PHASESHIFT-V3-SOTA-ARCH-001`  
**Date:** 21 November 2026  
**Target repository:** https://github.com/SaridakisStamatisChristos/PhaseShift  
**Baseline:** `main` at `279a8c65ad7ec3741a727ff49aa7f84e0628a6f1` (merged PR #5)  
**Verification at planning time:** exact merge-SHA GitHub Actions `build-test`, `sanitizers`, and `production-line-coverage` all **passed**.  
**Implementation:** C++20; CPU-only; offline; Linux x86-64 primary; Python permitted **only** for isolated benchmarking/adapters.  
**Status:** Forward-looking architecture/research specification, **not implemented**, **not evidence of SOTA**, and **not a proof of a novel algorithm**.

> **Execution record (23 November 2026).** The status line above describes the directive at planning time and is kept unchanged as history. The v3 program was executed on `feature/phaseshift-v3-sota-research` (PR #6) and merged into `main` with explicit owner approval.
>
> **Outcome:** engineering gates verified at exact SHAs. The one-time, pre-registered external confirmation against genuine SMAC3 2.4.1 and Optuna 4.6.0 TPE found **no advantage**, so **no SOTA claim is made**.
>
> See [§21](#21-execution-outcome-and-final-verdict-23-november-2026) and [`docs/v3/V3_CLAIM_AUDIT.md`](docs/v3/V3_CLAIM_AUDIT.md). This document is still a specification, not evidence; evidence lives in source, tests, Git history, CI runs and `docs/v3/`.

**Relationship to existing documents.** This plan advances—not replaces—the original `docs/PHASESHIFT_SPEC.md` and the prior `PHASESHIFT_V2_SOTA_UPGRADE_MASTER_PLAN.md`. All previously verified interfaces, scientific constraints, v1 defaults, v2 opt-in functionality and immutable evidence remain binding. Historical `docs/v2/*` reports may describe draft PR #5 as unmerged; they are historical snapshots, not current repository status.

---

# 0. One-shot implementation command for the next R7 / Trillion Developer++ session

> **Execute this entire PHASESHIFT v3 master directive against** `https://github.com/SaridakisStamatisChristos/PhaseShift`. Inspect live `main`, `AGENTS.md`, `docs/PHASESHIFT_SPEC.md`, the previous v2 directive, current CMake/CTest/CI and the evidence ledgers. Preserve verified behavior. Create `feature/phaseshift-v3-sota-research` from current main (or a non-conflicting equivalent). Implement complete, testable, minimal C++20 slices following the PR/milestone dependency graph in §13. For each milestone: implement; run appropriate local/CI tests; adversarially attempt to falsify correctness and performance; repair; commit atomically; push; update a draft PR and an evidence ledger. Move forward autonomously; do not ask for routine choices. Never merge to main without explicit owner authorization. Never force-push, modify unrelated user work, silently change old generator bytes/IDs, corrupt legacy SQLite evidence, weaken oracles, exploit holdout results for tuning, or claim SOTA without matched external evidence. If a research strategy loses, preserve the negative result and the existing winning fallback. Finish with an exact-SHA, claim-scoped scorecard, PR link, and remaining gates. No simulated test results, placeholder implementation, unexecuted CI claims or inflated benchmark comparisons.

**Interpretation of “SOTA”:** a *measured, scoped lead* on independently evaluated **boundary discovery per unit resource**—not universally the fastest optimizer, most effective fuzzer, or best changepoint detector. The program must be designed to falsify that claim, not assume it.

**Owner input needed:** None for normal execution. The repository URL is fixed. Existing validated default policies are the fallback. An owner decision is required only for privileged actions or for an explicit merge/release.

---

# 1. Current verified position: what v2 already does

## 1.1 Preserve these functioning capabilities

The current code provides an offline algorithm-behavior research engine with:

- CLI, isolated worker execution, bounded IPC, deterministic versioned case generation and independent correctness oracles.
- Built-in sorting, Top-K, graph and calibration families; a trusted native integer-array plugin ABI.
- Versioned SQLite experiment records, raw observations, deterministic decisions, replay/reproduce/minimize, source fingerprints and evidence verification.
- v1 baseline selection and opt-in `uniform_v2`, `stratified_v2`, `weighted_v2`, `hierarchical_boundary_v2`, `cliff_bracket_v2`, `hybrid_mixed_v2`.
- Lazy mixed/conditional feasible-space enumeration without materializing huge Cartesian products; deterministic sparse sampling.
- Experiment-level worker/parent time and measured CPU accounting, dual ceiling budgets, calibrated batch measurement option and drift descriptors.
- Exact multiple-change dynamic programming, PELT with bounded fallback, slope and jump screening, conditional 2×2 interactions, frozen independent factorial holdout and multi-hypothesis campaign machinery.
- Immutable authoritative evidence with optional verified derived model-cache sidecar; atomic recovery and adversarial crash checks.
- GCC/Clang, sanitizers and >95% line-coverage enforcement; the last R7 completion head recorded 93/93 CTests and 95.17% production line coverage, with all four R7-head workflows green. **The merged main itself has separately passed its three push workflows**, but no claim is made that its test-count report was independently extracted for this document.

**Observed research diagnostics on the R7 head** (previously fetched from actual GitHub workflow logs; not a new test run):

| Workload; 20 independent seeds; nominal max 8 discovery cases | Legacy `adaptive_partition` | Best cited v2 result | Interpretation |
|---|---:|---:|---|
| Synthetic single planted cliff, confirmed | 20/20 | `cliff_bracket_v2`: 20/20 | Both succeed; median width 2 for both, so no general improvement shown |
| Real Top-K, confirmed candidates | 5/20 | `uniform_v2`, `stratified_v2`, `hybrid_mixed_v2`: 14/20 | Potential improvement; correct truth status and fair resources need deeper adjudication |
| Real sorting, confirmed candidates | 0/20 | Tested v2 policies: 0/20 | Could be a legitimate absence of signal, not necessarily a search failure |
| Real graph, confirmed candidates | 0/20 | Tested v2 policies: 0/20 | Same qualification |
| Synthetic constant-null, confirmed | 0/20 | Tested v2 policies: 0/20 | Encouraging negative control, not calibrated global FWER |

**Critical comparability defect:** the existing challenge used only eight selected discovery cases and spent a small fraction of its ceiling on typical scenarios (about 5% of synthetic wall cap, about 15% of Top-K wall cap). Equal *limits* did not imply equal *consumption*. The real-workload boundary truth is not independently known for all cases. These findings are diagnostics, **not SOTA evidence**.

## 1.2 Concrete source-map risks

- `src/experiment.cpp` remains a large orchestrator with multiple policy selectors, query, persistence, confirmation and reporting coordination. Separate responsibilities without changing behavior.
- `include/phaseshift/scheduler_v2.hpp` scores interval contrast, geometry and historical CPU cost. Its “uncertainty” is explicitly **heuristic**, not a calibrated posterior or valid probability of discovery.
- Existing `src/boundary_v2.cpp` has serious detection algorithms, but its value is constrained by sparse, adaptively collected measurements and the need to compare multiple structure hypotheses.
- `src/interaction_v2.cpp` supports independent real-worker factorial checks, but the complete **campaign-wide** inference story and causal intervention interpretation remain limited.
- `include/phaseshift/model_cache_v2.hpp` is a derived sidecar; `run.sqlite` remains authoritative. Do not quietly promote a cache into a second source of truth.
- Current plugin support is **not** a universal arbitrary-binary loader, sanitizer-equivalent sandbox, MEV engine or GPU tool.
- The expensive v2 held-out GitHub workflow is PR-triggered / manually dispatched; a green push to main does not, by itself, mean every expensive research job ran on the merge commit.

## 1.3 What PhaseShift must beat

**Not** merely: an old fixed grid on one easy threshold. The direct scientific question is:

> Given a legal structural input space, a set of algorithms, a correctness oracle and a limited CPU/wall budget, which policy finds and **independently confirms** the most *true, localized, distinct* behavior boundaries, with the fewest false promotions and smallest end-to-end cost?

The system must beat strong v1/v2 internal baselines *and* credible external methods when those methods can be adapted to the same observation contract. Every result should say explicitly which objective it covers.

---

# 2. Product thesis and measurable SOTA target

## 2.1 Unique product capability

PhaseShift is a **boundary discovery laboratory**. Its product output is not a single optimizer incumbent; it is a compact, independently testable statement such as:

> “For `algorithm A` vs `algorithm B`, at input structure `condition C`, a measurable change occurs within boundary cell `R`, with raw matched-seed evidence, uncertainty and status qualifications, fresh confirmation samples, and a minimal independently replayable witness.”

It must also confidently say **NO_CONFIRMED_BOUNDARY** when a boundary is not supported.

## 2.2 State-of-the-art claim scope

Separate five claims:

1. **Engineering quality:** build/test/coverage/fault-containment verified.
2. **Boundary search capability:** finds designated synthetic/real boundaries under a defined budget.
3. **Statistical validity:** configured positive-attestation and family-wise error guarantees hold **under their actual assumptions**.
4. **Comparative performance:** beats the strongest relevant baseline in matched held-out trials, with uncertainty.
5. **External leadership:** strongest defensible measured result among a predeclared set of genuinely comparable systems. This is a **scoped**, time-stamped empirical statement; a general/universal SOTA claim is out of scope.

## 2.3 Objective vector, not one gameable score

For independent held-out *campaign* `i` and consumed budget `b`:

- `Y_i(b)`: number of **distinct, ground-truth-valid** boundaries confirmed by budget `b` (synthetic/known-truth only).
- `S_i(b)`: indicator at least one true boundary confirmed by budget `b`.
- `T_i`: consumed CPU/wall cost to first valid confirmation; **right-censored** at budget when absent.
- `E_i`: localization error (distance or interval width normalized by axis/feature scale) for matched ground-truth boundaries; non-detections also counted for recall.
- `P_i`, `R_i`: precision and recall with predeclared geometric matching/dedup criteria.
- `FP_i`: false positive **confirmations** on null/smooth/irrelevant controls.
- `O_i`: parent acquisition/model fitting overhead; `M_i`: peak memory; `Q_i`: replay and witness success.

**Primary scientific endpoint**: area under the curve `mean_i S_i(b)` over a **predeclared interval of consumed worker CPU** (and independently for host wall), with a report of `Y_i(Bmax)`. Report all outcomes, including zero discoveries and censored runs. For real examples without a ground-truth oracle, show **confirmed under the protocol**, never call it ground-truth true.

**Separate estimands:** case-count efficiency, worker-CPU efficiency, full host-CPU efficiency and elapsed wall-clock efficiency are different. Do not collapse them into a weighted number or treat equal ceilings as equal actual consumption.

## 2.4 Proposed promotion targets (hypotheses; NOT current measurements)

Targets to pre-register before touching evaluation data:

- **Reliability:** no regression in historical v1/v2 replay, schema, oracle behavior, wrong-answer handling or evidence seals; >=95% line coverage continues; sanitizer green.
- **Sustained performance:** on a broad held-out *known-truth* family, >=20% improvement in the primary success-budget AUC or >=20% less restricted mean consumed cost to a true confirmed boundary relative to the **best** eligible v1/v2 incumbent, with paired uncertainty and positive benefit across multiple task families. A positive effect on only one fixture must be reported as such.
- **Completeness:** non-inferiority on null false promotions and important non-cliff task groups, with predeclared margins. An uninformative small null sample is **not** evidence of non-inferiority.
- **Localization:** materially smaller mean matched normalized localization error and/or more distinct confirmed boundaries at the same consumed budget; prespecify margin and matching tolerance by scenario.
- **Practicality:** acquisition + fit + persistence overhead <10% full-run wall on a representative moderately expensive-worker set; for extremely cheap workers compare absolute overhead and a smaller deterministic policy fallback.
- **Statistical calibration:** empirical false-promotion rate with confidence bounds under specified independent null campaigns; report uncertainty. A 5% target does not mean that observing zero positives in a tiny sample proves a <=5% FWER.
- **External leadership:** only after matched, actually executed competition; the result may legitimately be “competitive but not leading.”

Never silently revise these targets after seeing held-out outcomes. Version the challenge instead.

---

# 3. Immutable constraints and compatibility boundary

**C01 – Historical identity:** old generator encodings and seeds, canonical case IDs, plugin ABI v1, v1 CLI defaults, v1/v2 policy names, old SQLite v7/v8/v9 and earlier bundle verification must remain semantically stable. New protocols have new versions, not silent edits.

**C02 – Oracle independence:** no self-comparison as correctness reference; exact status of wrong answer, crash, OOM, timeout, no sample and invalid IPC remains visible and cannot become a positive speed event.

**C03 – Confirmation isolation:** discovery models, policy tuning, candidate ranking and changes to acquisition weights cannot read or inspect the sealed confirmation/evaluation cohort.

**C04 – One authoritative store:** `run.sqlite` is source of truth. Model/prediction caches are reconstructible and independently verifiable; no cache result may overwrite raw observations.

**C05 – Deterministic logical replay:** deterministic seed/config/policy/model version/transcript → identical logical proposals, case IDs and replayed decisions on supported deterministic environment. Device times need not match bitwise.

**C06 – Atomic progress:** decisions, costs, observation status and budget accounting commit atomically at declared boundaries; pending worker re-execution is idempotent or explicitly logged.

**C07 – Resource limits:** search cannot exceed declared bounds without recording the specific atomic-case overshoot. Track child waited CPU and parent overhead; do not imply complete process-tree accounting until actually demonstrated on OS fault paths.

**C08 – Honest statistical scope:** fixed-sample intervals are not anytime confidence sequences; repeated looks and family selection need explicit handling; no causal interpretation from observational interaction alone.

**C09 – Offline C++20 core:** no mandatory API/LLM/Python/GPU/cloud service. Optional external benchmarking adapters run out of process.

**C10 – Evidence continuity:** never mutate prior raw rows or rewrite historical findings; append new versioned derived decisions. All losing/invalid candidates count in cost and audit.

**C11 – Safe experimentation:** model changes remain opt-in (`v3` policy/measurement/detection IDs). Feature flags allow immediate fallback to v1/v2.

**C12 – No novelty theatre:** use prior-art searches and comparable tests; report partial and negative outcomes, not invented formal complexity or universality.

---

# 4. Target architecture: five pipelines, clean ownership

```text
CLI / ExperimentCoordinator (stable public entry)
  |
  +-- Contract & Provenance ----------------------------------------------+
  |   ConfigValidator / VersionRegistry / CapabilityProfile / BudgetLedger |
  |   Immutable ExperimentPlan / frozen seeds & holdouts                   |
  +------------------------------------------------------------------------+
  |
  +-- Search ------------------------------------+    +-- Evidence ----------+
  |   FeasibleSpace -> CandidateFactory          |    | CommittedEvidenceView |
  |   StructuralKey + ReplicateKey               |<---| Cost & status index  |
  |   PolicyPortfolio -> AcquisitionPlanner      |    | Snapshot version     |
  |   Interval/CellIndex + Proposals             |    +----------------------+
  |   Model(s) + CalibrationDiagnostics          |
  +--------------------+-------------------------+
                       |
                       v
  +-- Execution & Measurement -------------------------------+
  | BudgetAdmission -> IsolatedWorker -> CorrectnessOracle    |
  | Pair/Block Scheduler -> raw observations/status/cost       |
  +--------------------+--------------------------------------+
                       |
                       v
  +-- Inference ----------------------------------------------+
  | CandidateBoundary(s) -> family freeze -> independent      |
  | confirmation -> multiplicity -> sign/shape verdict        |
  | -> multi-boundary witnesses -> separate falsification     |
  +--------------------+--------------------------------------+
                       |
                       v
  +-- Audit & Research ---------------------------------------+
  | SQLite authoritative event/decision log                  |
  | Derived model cache -> Replay / Reproducer / Exports      |
  | Frozen BenchmarkController / ExternalComparatorAdapters  |
  | ClaimLedger / CI / documentation                          |
  +-----------------------------------------------------------+
```

## 4.1 Ownership and forbidden edges

| Component | Owns | Must NOT own |
|---|---|---|
| `ExperimentCoordinator` | lifecycle, stable CLI dispatch, ordered transactions | policy mathematics, raw detection statistics, filesystem benchmark scripts |
| `FeasibleSpace` | valid structural coordinates, cardinality, legal transitions | timings, holdout labels, database transactions |
| `EvidenceSnapshot` | committed discovery-only summaries + provenance | live mutation or inferred success from failed observations |
| `SearchPolicyV3` | proposing legal candidates, deterministic policy state | invoking workers directly, reading holdout, mutating SQLite outside coordinator |
| `AcquisitionModel` | predictive ranking and uncertainty diagnostics | validity of experimental p-values or confirmation labels |
| `MeasurementEngine` | samples, run order, costs, failure status | boundary truth or scientific promotion |
| `BoundaryInference` | candidates, tested contrasts, conditional shape | choosing next discovery sample based on confirmation results |
| `ConfirmationRegistry` | frozen family, fresh independent outcomes, multiplicity | retroactive edits to discovery policy |
| `Store` | source transactions, append-only provenance, migration | selecting search strategy by hidden scoring |
| `BenchmarkController` | fairness contract and independent scoring | authoring the search policy or silently dropping failures |

Keep existing `src/runner.cpp`, `src/store.cpp`, `src/boundary_v2.cpp`, `src/interaction_v2.cpp` implementations wherever correct. Extract orchestrator pieces from `src/experiment.cpp` **using characterization tests** first. Avoid big-bang rewrites.

## 4.2 Proposed source layout (adapt names to the live tree)

```text
include/phaseshift/v3/
  types.hpp                 # immutable keys, typed objectives, statuses, evidence IDs
  budget.hpp                # dual+parent ceilings and event cost provenance
  discovery_view.hpp        # committed-only snapshots and indexes
  candidate_factory.hpp     # coordinate + replicate candidate formation
  policy.hpp                # SearchPolicyV3 contract and registry
  portfolio.hpp             # selected policy arms, resource allocation
  region_index.hpp          # 1D intervals, conditional cells, dedup
  cost_model.hpp            # robust worker cost prediction and calibration
  contrast_model.hpp        # seed-matched effect / noise / status models
  acquisition.hpp           # score, diversity, prospective value and fallback
  replicate_planner.hpp     # pair/block batch planning and seed choice
  boundary_models.hpp       # cliff, smooth, multi-cliff, status, crossover
  family_registry.hpp       # frozen claims, eligibility, multiplicity
  model_state.hpp           # reproducible derived versioned snapshots
  witness_predicates.hpp    # multi-boundary & conditional preservation
  research_metrics.hpp      # typed independent benchmark endpoints
src/v3/
  ... corresponding .cpp files only where they reduce compile/coupling cost ...
benchmarks/v3/
  scenarios.schema.json
  train_manifest.json
  evaluation_manifest.json        # sealed before tuning
  fairness_contract.md
  run_internal.py                  # uses real CLI and actual workers
  compare_external.py             # opt-in benchmark-only adapters
  scorecard.py                     # independent from C++ policy module
  verify_results.py
  adapters/{smac,optuna,ruptures,perffuzz}/
tests/v3/
  policy_characterization.cpp
  region_index_oracle.cpp
  acquisition_counterexamples.cpp
  cost_calibration.cpp
  paired_replication.cpp
  multidim_truth_oracle.cpp
  selective_inference.cpp
  campaign_fwer.cpp
  restart_model_replay.cpp
  worker_resource_faults.cpp
  witness_robustness.cpp
  benchmark_fairness.py
  ...
docs/v3/
  ARCHITECTURE_ADRS.md
  IMPLEMENTATION_LEDGER.md
  FALSIFICATION_LEDGER.md
  BENCHMARK_EVIDENCE.md
  EXTERNAL_BASELINES.md
  CLAIM_VERDICT.md
```

**Dependency direction:** `types <- space/evidence <- models <- policy <- coordinator`; inference consumes committed observations, not policy internals. Benchmarks invoke the public CLI rather than import or reimplement its private acquisition logic.

---

# 5. Versioned C++ contracts and replay architecture

The signatures below are design sketches, not a claim these declarations currently exist.

```cpp
namespace ps::v3 {
struct StructuralKey { std::vector<AxisValue> coordinates; uint32_t generator_version; };
struct ReplicateKey { uint64_t seed; uint32_t replicate_index; };
struct CandidateKey { StructuralKey structure; ReplicateKey replicate; };

enum class ProposalIntent {
    Explore, RefineBoundary, ValidateInteraction, ResolveNoise,
    DiagnoseFailure, Replicate, RandomFallback
};
struct ProposedTrial {
    CandidateKey key;
    ProposalIntent intent;
    uint64_t proposal_number;
    std::string model_hash;
    std::string policy_version;
    std::string provenance_reason;
};
struct ObservationSummary {
    CandidateKey key;
    std::string status;  // not a numeric infinity/speed for failure
    std::optional<double> paired_log_ratio;
    std::optional<double> robust_dispersion;
    uint64_t measured_child_cpu_ns;
    uint64_t parent_cpu_ns;
    uint64_t total_wall_ns;
    std::string provenance_digest;
};
struct BudgetSnapshot {
    uint64_t child_cpu_ns, parent_cpu_ns, host_wall_ns;
    uint64_t evaluated_cases, attempted_cases, invalid_cases;
    uint64_t hard_worker_mem_bytes;
};
struct DecisionContext {
    const FeasibleSpace& space;
    const DiscoveryEvidenceSnapshot& discovery;
    BudgetSnapshot budget;
    uint64_t deterministic_epoch;
};
struct SearchPolicyV3 {
    virtual ~SearchPolicyV3() = default;
    virtual std::optional<ProposedTrial> propose(const DecisionContext&) = 0;
    virtual void on_committed(const ObservationSummary&) = 0;
    virtual std::string canonical_state_digest() const = 0;
};
}
```

### Rules

1. Real implementation may optimize away large copies with handles/views; never return dangling views across SQLite transaction boundaries.
2. `StructuralKey` identifies *structure*, while `ReplicateKey` identifies independently seeded evaluation of that same structure. Duplicate detection operates on the **pair** for repeated-measurement mode and the structure where no replicate is requested.
3. Identical structure with different fresh seeds is intentionally **not** a duplicate trial. Preserve old v1/v2 case ID mapping and do not change source generator semantics.
4. Every proposal records origin arm, model hash, ranking diagnostic, random state, feasibility proof/check, expected cost, and actual consumed cost after execution.
5. An optional acquisition model can produce **ranking scores** without representing a calibrated probability. Probability-valued methods require separate actual calibration evidence.
6. `propose()` cannot inspect a holdout table; enforce by construction, separate read handles/views and assertions.
7. New v3 model state must deterministically reconstruct from source observations + canonical versioned parameters, or persist a verifiable snapshot (hash + immutable source-position watermark). No time-based seeds.
8. Canonical serialization requires stable byte order, stable integer/floating-point handling, strict non-finite rejection and documented math/toolchain reproducibility limits.

---

# 6. Core algorithmic strategy: Active Boundary Intelligence (ABI-v3)

**Architecture name** (internal only): `ActiveBoundaryIntelligenceV3`. This is a hypothesis-driven **portfolio**, not an unproven new theorem or an automatic requirement that Bayesian optimization is superior.

## 6.1 Why a portfolio, not one giant GP

Tasks differ sharply: planted 1D cliffs, categorical XOR interactions, resource failures, smooth crossovers, noisy repeated measurements and giant constrained spaces require different exploration. A single global Gaussian process over millions of mostly categorical points can be computationally expensive and poorly matched to discontinuities. Use **deterministic low-overhead specialist arms**, a calibrated cost layer, and a guarded optional surrogate.

### Candidate arms

| Arm | Appropriate regime | Mechanism | Fallback |
|---|---|---|---|
| `cliff_bracket_v2` | 1D abrupt shift | Existing deterministic bracketing/contrast | already validated v2 |
| `axis_slice_v3` | Mixed space with one ordered dominant axis | Conditional slice search, neighbor refinement | hierarchical v2 |
| `cell_frontier_v3` | Multiple ordered dimensions | Best-first adaptive hyperrectangle partition, only legal cells | stratified v2 |
| `interaction_screen_v3` | Conditional / categorical interactions | Seed-matched two-factor contrasts and rare-cell targeting | existing factorial v2 |
| `noise_resolve_v3` | Noisy near-threshold contrasts | New independent matched-seed replications | fixed replicate plan |
| `status_frontier_v3` | Crash/OOM/timeout/wrong-answer boundaries | Status classification search; never treat as timing | ordinary status evidence |
| `uniform_v2` / `stratified_v2` | Exploration / adversarial backup | Unbiased reserve, no candidate-prior cheating | baseline |
| `surrogate_rank_v3` (opt-in) | Enough varied valid training samples | Robust feature-based tree/ensemble or local regression; trained on discovery only | disable if uncalibrated/slower |

### Regional data structure

Implement `RegionIndex` as a hierarchy of *legal* cells, not an eager full lattice. Each region stores axis spans/categorical constraints, observed corner/interior evidence, regional contrast, status mixture, model fit version, estimated cost and refinement eligibility. Heap entries are lazy-invalidated with generation numbers; child cells inherit legal constraints and never synthesize impossible coordinates.

**Complexity target:** proposal update `O((d + k) log R + Cmodel)` for `d` changed dimensions, `k` small affected adjacent cells, `R` active regions, plus bounded candidate construction; `Cmodel` must be explicitly measured and capped. Do **not** claim a worst-case bound if cell splits or conditional feasibility require an unbounded scan. Memory `O(R + E + model_state)`, not proportional to virtual full-grid cardinality `N`. Small finite domains must be exhaustively differential-tested against an independent feasible-space enumeration oracle.

### Region splitting

- Ordered axis: split at legal median or a contrast-conditioned index with deterministic ties.
- Categorical axis: split by explicit legal group identity, not arbitrary numeric order.
- Conditional space: inherit conjunction constraints and project candidate coordinates through existing admissible rectangle machinery, keeping disjointness and overflow checks.
- Sparse mixed case: use balanced per-axis low-discrepancy/stratified warmup, then local cell focus.
- Avoid geometric-volume bias: a huge low-information cell is not automatically more valuable than a small rare interaction.
- Bound active cells, queue size, recursion depth, model fitting time and daily trial count. On cap, freeze the most informative summaries and fall back to stratified proposals.

## 6.2 Cost model: actual cost, not made-up wall-time preference

Build a robust, hierarchical predictive model for `worker_cpu_ns` and full elapsed wall. Suggested features: family, input length/vertex count/edge count/`k`, algorithm IDs, measurement protocol, past samples, valid/failed status and observed batch size. Use robust medians + shrinkage for cold categories; only introduce a more complex predictor if cross-validation improves held-out calibration.

```
PredictedCost(x) = robust median cost among similar valid committed discovery cases
                  + explicit fixed parent/serialization/pilot cost estimate
```

For budgeting, use an *upper prediction bound* calibrated on held-out **discovery-training** costs (e.g., a conservative quantile) when sufficiently supported; report coverage and how failure paths change it. Never state a distribution-free bound for arbitrary adaptive dependent sequences. If predictive coverage is inadequate, use the empirical worst-case bounded-worker estimate or pessimistic cap.

**Cost must not be confounded with speed objective:** a region expensive to evaluate could reveal the most valuable cliff. Cost normalizes *information per resource*; keep explicit exploration reserve for high-cost important regimes.

## 6.3 Contrast / uncertainty models

Provide **two different numbers**:

1. A deterministic *ranking uncertainty* score for scheduling (may be heuristic).
2. A calibrated predictive interval or posterior uncertainty **only if validated** on frozen held-out replicates and correct assumptions.

For positive performance samples, use paired differences of robust transformed measures, e.g. `d = log(time_A) - log(time_B)`, matching seed, input structure and measurement block. Keep uncertainty across **independent seeds and sessions**, not just repeats within a single tightly coupled run. Heteroscedastic noise and status censoring require separate treatment. Drift scores are covariates/diagnostics, not automatic valid p-values.

### Candidate acquisition

Initial explainable score (not a theorem):

```
value(x) = w1 * boundary_contrast(x)
         + w2 * expected_uncertainty_reduction(x)
         + w3 * novelty/diversity(x)
         + w4 * interaction_potential(x)
         + w5 * status_frontier_value(x)
score(x) = value(x) / max(eps, predicted_total_cost(x))^alpha
```

All feature transforms dimensionless and clipped, weights fixed on the development split. The scoring function **must not** be interpreted as a valid probability merely because it has a `value` term. An optional more principled `expected_reduction_in_boundary_entropy / cost` acquisition may be evaluated only if the boundary model supports a validated probability distribution; exact/approximate computation budget and numerical stability must be tested.

### Portfolio arm selection

Start with a **fixed deterministic allocation** (example design candidate: 25% global exploration, 45% local boundary refinement, 20% paired replication, 10% interaction/status screening; percentages tuned and frozen on training only). Define adaptive arm allocation as a separate ablation, e.g. delayed-reward contextual bandit with explicitly observed *development* rewards; do not claim unbiased online reward or independence from selection. Preserve a minimum forced exploration fraction. If adaptive allocation loses or becomes noisy, keep the fixed portfolio default.

**Crucial:** `cliff_bracket_v2` and `hybrid_mixed_v2` are strong incumbents. New v3 policies must defeat them head-to-head rather than merely defeat the weakest legacy policy.

## 6.4 Replicate planner (high-value first implementation)

- Choose a structural coordinate pair `(xL,xR)` and **fresh shared replicate seed** (where generator supports semantically paired seeds).
- Persist the pair as a single measurement design with two real workers / statuses; if either fails, record both with the appropriate censoring/eligibility.
- Randomize execution order within matched block; counterbalance order across blocks; keep correct v1 ABBA/BAAB semantics.
- Allocate replicates when estimated effect uncertainty makes the proposed boundary ambiguous, *not* just because one location is promising.
- Predeclare maximum discovery replications; **all confirmation** replicates remain independently frozen.
- Avoid reusing holdout seeds for any future policy training or further searching within the same registered campaign.
- Test paired-seed effects against intentionally mismatched-seed controls and non-exchangeable generator scenarios; state pairing assumptions.

**Adversarial example:** two adjacent structures have identical true performance but different default per-point RNG seeds. A non-paired sampler can hallucinate a cliff; a correctly paired search should reduce that specific confounding without promising to remove every form of noise.

## 6.5 Rare region and high-dimensional constraints

1. For `d <= 3` with reasonable occupancy, use region hierarchy and conditional slices.
2. For many dimensions, use stratified screening, pairwise interaction nomination and sparse subspace refinement rather than exponential factorial search.
3. Limit interaction order to two by default; support higher order only via a separate complexity-budgeted, independently evaluated feature flag.
4. Preserve nominal categorical semantics; encode an explicit embedding for optional surrogate ranking only with documented invariances.
5. Enforce sampling **uniform over legal candidates** where claimed; validate conditional rectangle weights and rare-category representation.
6. Provide a hard constraint-complexity error rather than silently enumerating or rejecting an unbounded number of illegal coordinates.

---

# 7. Boundary Intelligence: shape-aware, status-aware, multi-boundary

## 7.1 Hypothesis taxonomy

Give every candidate a typed label and separate scientific interpretation:

- `ABRUPT_CLIFF`: localized jump in valid performance metric.
- `CROSSOVER`: sign of paired A−B effect changes across legal structure.
- `SMOOTH_TRANSITION`: broad slope/change, **not** automatically a cliff.
- `MULTI_CLIFF`: two or more separable localized transitions.
- `CONDITIONAL_INTERACTION`: effect exists in specific structural condition/slice.
- `SCALING_KNEE`: finite-range empirical change in fit/slope; **not** Big-O proof.
- `STATUS_FRONTIER`: correctness, resource or timeout status changes; not a timing comparison.
- `UNSTABLE_OR_INCONCLUSIVE`: credible artifacts but insufficient independent evidence.
- `NO_CONFIRMED_BOUNDARY`: valid negative outcome.

Existing v2 exact DP, PELT, jump, slope and campaign logic are *reusable detector backends*, not candidates for rewriting without evidence.

## 7.2 Compare rival shape explanations

On a discovery-only set, score at least: constant/null, smooth monotone, step, piecewise linear, multi-step, localized outlier and heteroscedastic noise explanations. Prefer a transparent penalized model with exact small-array reference oracle and bounded approximation on large data. Never claim a model-selection penalty provides post-selection significance on its own.

For multivariate mixed data, nominate 1D/2D conditional slices and evaluate their residualized matched comparisons; do not impose a fake total order on categorical variables. Differentially test projected slice boundaries against independent enumerated tiny-domain truth.

## 7.3 Adaptive sampling bias and selective inference

Discovery points are chosen *because* their observed metrics are extreme or uncertain. A standard confidence interval on reused discovery data is generally not a valid fresh confirmation interval. Therefore:

1. Discovery model generates candidates and provisional localization.
2. Freeze a specific claim: kind, region, comparator, effect direction, practical threshold, cohort, feature predicate, family membership, null definition.
3. Independently draw never-used confirmation seeds/points according to the frozen protocol; do not move the boundary after seeing confirmation outcomes.
4. Apply a justified test and family-wise adjustment to **all eligible registered hypotheses**, including losses and rejected candidates.
5. Report conditional/slice scope and abstention when evidence is insufficient.

### Localization uncertainty

Do not confuse the smallest observed interval with a statistically calibrated interval. To report coverage, use independently planted boundary datasets and measure the proportion of truth contained in a frozen reported region, with empirical confidence bounds. If a formal confidence set is requested, derive and implement a valid procedure under a specified stochastic model and test its assumptions. Otherwise label intervals **descriptive**.

---

# 8. Statistical engine: prevent attractive false claims

## 8.1 Explicit family registry

Every campaign must persist its predeclared `family_id`, sampling frame, frozen hypothesis list and deterministic ordering, tested metrics, holdout plan, alpha allocation, multiplicity correction and source SHA. Child hypotheses added later belong to a **new** campaign/family, not retroactively to the old family.

- For valid independent fixed-sample tests, retain Holm family-wise control; test type and null assumptions must be explicit.
- For seed-matched pair contrasts, use a sign/permutation test **only if** its exchangeability or sign-symmetry assumptions genuinely hold. Do not equate `n=20` with an automatic 99%-confidence guarantee.
- Avoid treating repeated timings within one worker as independent samples; group at the right seed/session unit.
- For dependent/adaptively chosen families, rely on properly frozen independent holdout and valid family tests, or explicitly develop a mathematically justified alternative with proof and simulation. No opportunistic optional peeking.
- Separate *confirmation p-values*, discovery ranking probabilities, approximate model scores and empirical calibration rates in schema/CLI/report.
- Record invalid/failed/abstained hypotheses. Do not remove them from the family denominator post hoc.

## 8.2 Null and negative control calibration

Build a *real engine* correlated-null suite, not just 3,000 standalone unit-level draws:

- same observed structure and paired seeds, with independent null outcomes.
- temporal drift and autocorrelation across adjacent measurements.
- heteroscedasticity, outliers, heavy tails, categorical imbalance and rare legal regions.
- simulated status censoring where wrong answers must never become a timing win.
- many adaptively selected candidates passed to a sealed confirmation family.
- repeated campaigns executed with realistic worker, SQLite, resume and model-selection logic.

Report empirical false-family-promotion frequency, exact/numerically justified binomial uncertainty where independent campaigns are valid, and separate dependence stress results. Do not use a single favorable run to certify a population error rate.

## 8.3 Optional anytime inference

Do **not** import an e-process/e-value/sequential test solely because it is fashionable. Introduce only if needed for flexible stopping; require an explicitly stated null, safe conditional expectation conditions, a derivation/referenced theorem that actually applies, adversarial simulation, integration tests, and independent review. Until then, the fixed frozen confirmation protocol is the reference.

---

# 9. Evidence, persistence and robustness

## 9.1 Ledger and schema

Create additive v3 tables or event records *only if* the live v9 layout cannot express the needed linkages. Candidate additions:

```text
v3_policy_versions(id, policy_name, source_sha, hyperparams_hash, model_contract)
v3_candidate_structures(structure_key, canonical_coords, generator_version)
v3_replicates(replicate_key, structure_key, seed, source_family, purpose)
v3_policy_events(seq, source_snapshot_hash, proposal, arm, model_hash, score_trace)
v3_model_versions(model_hash, source_row_watermark, serialized_parameters, validity)
v3_budget_events(seq, worker_cpu, parent_cpu, wall, rss, overshoot_reason)
v3_claim_families(family_id, predeclared_spec_hash, status)
v3_claim_members(family_id, hypothesis_id, registered_order, holdout_ref)
v3_benchmark_claims(claim_id, frozen_manifest_sha256, eligible_baselines, verdict)
```

Enforce referential integrity, strong uniqueness, monotone events, source snapshot hashes and strict migration acceptance. Schema version increases from the **live verified schema**, not from a stale documentation assertion. Preserve read-only projection of old v7/v8/v9 runs.

## 9.2 Crash and process accounting matrix

Inject SIGTERM/SIGKILL at: before proposal creation, after proposal creation, before worker, during worker, after worker before result commit, during SQLite transaction, after transaction before cache checkpoint, and during model rebuild. Also test: worker spawns descendants, grandchild escapes, timeout, OOM, corrupt IPC, busy/corrupt DB, disk full, killed parent, altered model cache, absent plugin, changed executable SHA and lost artifact.

**Acceptance:** no forged valid sample or double counted cost; deterministic logical replay where the contract promises it; explicit `ACCOUNTING_INCOMPLETE` when child descendants cannot be attributed by the available OS mechanism. Linux `wait4` plus parent counters is not proof of fully observed entire process tree. Optional cgroup v2 harness can be used **only when available** and its extra privileges/setup documented; keep fallback behavior honest.

## 9.3 Minimize an actual boundary, not only a failure

Extend the existing independent witness machinery to preserve a typed predicate:

- both endpoints valid and oracle-correct;
- frozen boundary kind/direction and practical effect threshold;
- same conditional categorical/axis scope;
- at least two distinct structural regimes on either side;
- prescribed fresh independent replicate support (not reused search seeds);
- maintained source linkage to the original candidate and every attempted shrink.

Implement a budgeted two-level minimizer: structural params then literal bytes where supported. Every attempted shrink and failed oracle decision is evidence, not something to hide. Recompute a deterministic, independently checkable signed manifest. If no smaller witness exists under the available budget, preserve the original.

---

# 10. Measurement and experiment-control architecture

## 10.1 Preserve timing semantics

Keep legacy `legacy_retained_outputs_v1` and v2 calibrated batch as explicit protocols. Define v3 additions only as opt-in and separately evaluated:

- pilot calibration, warmup, timed region, post-timed correctness checks, allocation retention and worker startup are distinct accounted phases;
- balanced ABBA/BAAB or other randomized counterbalanced measurement blocks are persisted;
- record CPU affinity, scheduler policy, governor when observable, compiler, stdlib, kernel, CPU topology, turbo/frequency diagnostics where permitted;
- do not alter host settings without explicit authorization; report an environment warning when uncontrollable;
- reserve real timed-region samples for sufficiently large batch durations; noisy short kernels need robust dispersion and independent sessions;
- worker status/censoring never gets a synthetic finite runtime score;
- profiling/coverage instrumentation must not silently contaminate the measured production timings.

## 10.2 Actual dual-budget admission

Budget manager checks hard bounds before scheduling each atomic case; validates expected upper cost, refuses an inadmissible new case, and records any necessary post-case overshoot exactly. Include the parent sampler/model fitting/candidate-generation CPU in a separate full-system CPU metric. The wall clock starts before initial policy setup and ends after confirmation/report when the claimed metric includes those stages.

**Important benchmark correction:** if a workload naturally finishes after eight samples, a 15-second cap does **not** make it a 15-second test. V3 studies must run budget-saturating or sufficiently dense workloads where possible, plus report per-consumption performance curves. Compare at *common observed consumed costs* and separate matched-case-count runs. Do not falsely assert policies consumed equal CPU merely because they shared max limits.

## 10.3 Hardware studies

For final candidate leadership claims, repeat on at least two different CPU/memory configurations where practical, using one independent reference host and one CI host. Cross-host absolute timings are not comparable as if they were same-machine matched repeats. Compare within-host policy ratios, then study heterogeneity of the ratio with host as a factor. If only a CI runner is available, narrow the claim.

---

# 11. Scientific comparison harness: independent from the engine

## 11.1 Frozen scenario matrix

Training/development, sealed evaluation, and adversarial falsification suites must be **disjoint in generator seeds and scenario parameters**. Holdout examples should challenge extrapolation rather than reuse the same planted threshold with renamed seeds.

| Family | Ground truth | Failure mode targeted | Measurements |
|---|---|---|---|
| Single cliff, multiple positions | Exact planted location | old boundary heuristic failure | recall, width, CPU-to-confirm |
| Two/many disjoint cliffs | Exact set | only one boundary found | distinct recall, precision |
| Smooth/no cliff | Known no abrupt step | hallucinated jump from slope | false confirmations |
| Constant/correlated null | Known none | selection + peeking | empirical family FWER |
| Rare thin interaction / XOR | Exact conditional cells | dominant-axis bias | conditional recall |
| Categorical mixtures | Exact label-specific change | fake categorical ordering | localization correctness |
| Heteroscedastic/heavy-tail | Planted or no truth | noise mistaken for boundary | calibration/abstention |
| Drift/order confound | Known seeded control | time-dependent false contrast | status and false-promotion |
| Expensive true boundary | Known location | overaggressive cost normalization | yield vs CPU |
| Status frontier | Known status boundary | wrong-answer/timeout as fast | zero invalid promotion |
| Sorting real workers | Empirical, not generally truth-labeled | overfitting to synthetic | confirmed claims, replication |
| Top-K real workers | Empirical, not generally truth-labeled | weak incumbent | confirmed claims, replication |
| Shortest-path real workers | Empirical, not generally truth-labeled | topology/weight restrictions | status, cost, confirmed claims |
| External C++ plugin targets | Grounded source/release semantics | adapter bias | separate benchmark scope |

Build at least three *qualitatively distinct* landscapes per synthetic family. Avoid just randomized replications of the same shape. Choose scenario parameters and matching tolerances **before** final evaluation.

## 11.2 Genuine competing baselines

**Internal:** legacy `grid`, `random`, `stratified`, `adaptive_partition`; `uniform_v2`, `stratified_v2`, `weighted_v2`, `hierarchical_boundary_v2`, `cliff_bracket_v2`, `hybrid_mixed_v2`; weak/strong baselines and carefully defined oracle-informed upper bound only as a diagnostic (an oracle is not eligible as a competitor).

**External mode A — model-based black-box acquisition:** SMAC3 and an Optuna TPE policy on the *same typed legal candidate space*, using a benchmark-only adapter that invokes the exact PhaseShift worker+oracle or a shared immutable target evaluator. Match allowed warmup, adaptive observation feedback, seeds, trial/CPU/wall limits and retry accounting. Black-box optimization typically seeks a best objective value—not an entire boundary—so use a shared **explicitly defined boundary-oriented acquisition reward** and label any proxy adaptation. Include unadapted original optimization objectives as separate, non-direct diagnostics. Python runtime cost counts in end-to-end CPU/wall comparisons.

**External mode B — detection:** `ruptures` PELT / suitable detector on the **same frozen, fully observed or appropriately masked time series** with the same cost function, penalty choice procedure, missingness and target number-of-change assumptions. This comparison is **detector-only**; it cannot establish superior adaptive sampling.

**External mode C — performance fuzzing:** PerfFuzz/SlowFuzz/AFL++ variants **only** on compatible instrumented external programs with the same maximization/objective, input validity, source builds and bounded resources. Score pathological-input discovery separately from boundary localization/confirmation; if objective alignment is impossible, say `INCOMPARABLE` instead of scoring a victory.

**Optional:** BOHB/Hyperband on a legitimate multi-fidelity cost contract; do not treat smaller inputs as a harmless low-fidelity version if the underlying cliff disappears or shifts. Explicitly establish fidelity correlation or exclude that comparator.

**License/source hygiene:** benchmark competitors as independent processes; do not copy GPL/AGPL code into PhaseShift. Pin release versions, source commits, benchmark harness licenses and hashes.

## 11.3 Fairness protocol (must be enforced by code)

1. Freeze `benchmark_plan.json` with SHA-256 before reading any evaluation results: scenarios, seeds, objective, truth labels, model parameters, time/case ceilings, multiplicity plan, inclusion rules, comparable baselines.
2. All policies receive an **equivalent initial evidence** set and legal candidate space. Reserve each policy's own search observations from peeking by others unless explicitly measuring shared-data policy optimization.
3. Randomize/interleave policy execution order **within each scenario and host/session**; avoid always running v3 at the coolest CPU time.
4. Compare matched **case limits**, matched **attempt caps**, and matched **consumed CPU/wall horizons** as separate named experiments.
5. Charge model training, interpreter startup, input generation, correctness oracles, storage and confirmation according to the declared end-to-end objective. Display worker-only and full-system results side-by-side.
6. Never drop timeouts, OOM, wrong answers, invalid candidates, worker startup cost, failed external adapter cases, missing trials or boundary-ceiling overshoot. Record precise censored statuses.
7. Report selection/tuning budget separately. Hyperparameter search compute is not free; freeze before held-out evaluation.
8. Use >=20 independent evaluation seeds/scenario variants as an initial floor **where computationally feasible**, not an automatic guarantee of statistical power. Predeclared sequential extension only with valid stopping analysis; otherwise increase to a prespecified fixed sample count.
9. Use paired uncertainty across *independent campaign units*, not individual repeated wall-clock observations. For `S(B)` report proportions with confidence bounds; for consumed cost show right-censoring and a justified restricted-mean estimator where appropriate.
10. Stratify by real and synthetic workload, dimensionality, presence/absence of a genuine cliff and resource cost. Do not let an easy common group drown rare tasks.
11. Publish all seed-wise results, errors, null findings and CI links, not merely a top-line aggregate.
12. Require an independent checker to verify scorecards from raw output + frozen manifests and reject mismatched seals/hashes.

## 11.4 Primary plots and tables

- Cumulative probability of first valid confirmed true boundary vs **consumed worker CPU seconds** and vs **host wall seconds** (right-censored, all campaigns).
- Mean number of unique true confirmed boundaries vs consumed resource.
- Exact/normalized localization error conditioned on a reported match **together with** recall; never show only successful matches.
- Null family confirmation rate and confidence bounds.
- False-status promotion count (must be zero on verified fixtures).
- Pareto plot of true discovery vs full CPU, wall, peak RSS and failure count.
- Per-family head-to-head paired differences and confidence bands; an aggregate is secondary.
- Prediction calibration graph for cost/uncertainty; overhead vs worker expense.
- Ablations of cost weighting, replication, interaction, cell hierarchy, surrogate, diversity and portfolio adaptivity.

**Anti-gaming oracle:** an independent reference evaluator can exhaustively enumerate *small* synthetic domains to derive truth, but the actual search engine must never read those labels during discovery. For huge legal spaces, truth must come from analytically planted generators or independently evaluated slices, not inferred from engine-selected winners.

---

# 12. Adversarial falsification scenarios

For every new policy, deliberately test the following before accepting it:

- **Late cliff:** only at the last valid point; endpoints may be misleading.
- **Double cliff:** changes cancel globally; midpoint/easy split sees nothing.
- **Narrow island:** only one or two points differ, far from largest interval.
- **Noisy null with apparent isolated jump:** must resist positive confirmation.
- **Expensive true cliff:** cost-normalized greedy policy ignores the important location.
- **Many cheap false cliffs:** search gets trapped and burns every confirmation.
- **Categorical XOR:** no main effect, only interaction; fake ordinal encodings mislead.
- **Rare category / skewed feasible volume:** selection bias hides discovery.
- **Smooth sigmoid or polynomial trend:** step detector overstates a cliff.
- **Input-seed confounding:** adjacent positions have unrelated random draws.
- **Generator status asymmetry:** one algorithm crashes/returns wrong answer, other valid; must not become speed crossover.
- **Very large grid / constraint overflow:** no eager enumeration, silent integer wrap or unbounded rejection loops.
- **SQLite cache poisoning:** mismatch must reject/repair from authoritative data only.
- **Restart between every atomic event:** same logical decisions; no model RNG drift.
- **Zero/negative/NaN measurement or missing CPU counters:** fail closed; no division-by-zero reward.
- **Policy warm-start leakage:** validation seeds inadvertently appear in training data.
- **Host drift:** policy order confounds apparent advantage.
- **Comparator overhead:** external Python adapter wins worker-time-only but loses full-system budget; report both.
- **Unmatched objectives:** PerfFuzz maximizes hotspot cost, PhaseShift localizes boundaries; must not issue an ineligible aggregate rank.

Each falsifier has a stored minimal reproducer and a regression test if it reveals a real bug. Failed attempts are documented with the exact setting, not quietly discarded.

---

# 13. Autonomous milestone / PR dependency graph

**Priority principle:** benchmark validity and the paired-replication discovery loop before sophisticated learned acquisition; cost-aware model after honest cost measurements; modular refactor before a giant new coordinator. Small reviewable PRs, one feature contract at a time.

```text
V3-00 verify/refresh ground truth + historical provenance
   |
   +-- V3-01 independent benchmark harness + consumed-budget curves
   |      |
   |      +-- V3-02 orchestrator extraction + stable v3 interfaces
   |             |
   |             +-- V3-03 seed-matched replicate planner + evidence keying
   |             |      |
   |             |      +-- V3-04 cell/axis conditional search and portfolio
   |             |             |
   |             |             +-- V3-05 cost+uncertainty acquisition/calibration
   |             |                    |
   |             |                    +-- V3-06 optional learned surrogate
   |             |
   |             +-- V3-07 boundary taxonomy & multi-boundary/stats
   |                          |
   |                          +-- V3-08 campaign FWER, false positive study
   |                                      |
   |                                      +-- V3-09 witness + recovery hardening
   |                                                   |
   +---------------------------------------------------+-- V3-10 external comparison
                                                         |
                                                         +-- V3-11 release verdict
```

## V3-00 — Baseline integrity and document truth (P0)

**Work:** inspect live `main` (not hardcode prior SHA for implementation), all CI and acceptance smoke; archive actual v2 scorecards and R7-head research artifact hashes; update `docs/v2/` current-state notices without falsifying historical checkpoints. Pin new `baseline.json` with commit/version/CI run IDs and current software environment. Create `docs/v3/IMPLEMENTATION_LEDGER.md`.

**Gate:** exact live code baseline green; no false statement that post-merge research heldout was rerun; old run/schema fixtures replay. **Commit:** `docs(v3): freeze merged baseline and research gaps`.

## V3-01 — Independent experimental benchmark framework (P0; FIRST CODE CHANGE)

**Work:** pre-registered disjoint manifests, independent truth oracle for small known cases, end-to-end actual consumed CPU/wall accounting, budget-saturating case counts where sensible, run-order randomization, replayed CLI raw logs, censoring, fairness checker, `S(B)` and null/reliability reporting. Default no Python runtime dependency in PhaseShift core. Add metadata and failure outcomes without opportunistically changing baseline policies.

**Gate:** exact old v2 schedules rerun with unchanged identities; can independently detect under-saturated 8-case caps; benchmark self-test catches fabricated equal-consumption claim and missing failure rows; >=20 full known-truth seeds per eligible primary scenario or a clearly marked incomplete research plan. **Commit:** `bench(v3): freeze fair consumed-resource evaluation`.

## V3-02 — Architecture extraction and typed contracts (P0)

**Work:** extract coordinator, policy factory and evidence snapshot from `src/experiment.cpp` with golden tests; define typed candidate/replicate IDs and budget admission. Keep v1 and v2 policy code untouched; wrap adapters. Existing `run.sqlite` authoritative.

**Gate:** byte-identical legacy case identity and v2 golden proposals on fixed input/seed; exact CLI/schemas report compatibility; no line coverage regression; no allocator/performance regression above predeclared tolerance in warmed baseline. **Commit:** `refactor(v3): separate orchestration search and evidence contracts`.

## V3-03 — Seed-matched paired adaptive replication (P0)

**Work:** explicit structural vs replicate identity; new discovery-only paired contrasts, stored design, robust dispersion, choice of replicate or new coordinate under budget; fresh independent confirmation untouched. Implement and test on real worker CLI, not only in-memory simulation.

**Gate:** paired vs unpaired noise-confounding fixture; replay/resume fully identical; no false status promotion; explicit independent cohorts; no use of holdout data. **Commit:** `feat(v3): schedule fresh matched-seed discovery contrasts`.

## V3-04 — Conditional region tree and hybrid search portfolio (P1)

**Work:** legal cell partition, per-axis boundary search, categorical / rare region screening, lazy invalidation, diversity, bounded queue, forced global exploration and mixed-dimension conditional legality. Portfolio initially fixed allocation and deterministic.

**Gate:** exhaustive oracle over small finite legal domains, 10^9+ virtual-space small RSS challenge, seeded schedule and restart, adversarial double-cliff/XOR/rare island comparisons, measured parent overhead. Promote policy only if it improves discovery in predeclared development trials; v2 remains opt-in fallback. **Commit:** `feat(v3): add legal cell frontier and policy portfolio`.

## V3-05 — Predictive cost and uncertainty calibration (P1)

**Work:** robust cheap baseline; optional quantile cost bounds; held-out discovery-training calibration; explicit score separation; guard high-cost true cases; `acquisition_v3` behind flag. Predeclared ablations `no_cost`, `no_uncertainty`, `no_replicate`, `no_diversity`, `no_interaction`.

**Gate:** no calibration claim without actual coverage data; cost prediction does not change worker measurement semantics; model misspecification triggers deterministic fallback; measurable policy gains on multi-family development suite with overhead budget. **Commit:** `feat(v3): add auditable cost and uncertainty acquisition`.

## V3-06 — Optional surrogate challenger (P2; killable research branch)

**Work:** tiny deterministic regularized model / tree ensemble trained on committed discovery; compare against heuristic v3 portfolio and direct SMAC/TPE adapters; bounded CPU/model state. Do not require a GP, torch, Python or new deep ML runtime. Treat numerical nondeterminism explicitly.

**Gate:** improves held-out scenario family against simpler v3 acquisition with net end-to-end cost, stable calibration and no worse null/rare regime; otherwise **do not ship as preferred policy**. **Commit (only if passed):** `feat(v3): add verified surrogate challenger`.

## V3-07 — Boundary taxonomy and multi-hypothesis localization (P1)

**Work:** reuse v2 DP/PELT/jump/slope, implement robust rival-shape scoring, sparse/missing-aware conditional projections, exact small oracle, status-frontier classification and interaction discovery. Keep inference clearly separated from scheduling.

**Gate:** injected ground-truth multi-cliff/smooth/outlier/calibration, no false `ABRUPT_CLIFF` from smooth-only truth, simultaneous multiple distinct boundary matching and uncertainty diagnostics. **Commit:** `feat(v3): classify conditional and multi-boundary hypotheses`.

## V3-08 — Independent statistical calibration and campaign closure (P1)

**Work:** actual full CLI/SQLite correlated-null campaigns, multiplicity family state machine, sign/permutation assumptions, selection/holdout leakage tests, drift tests, positive and negative campaign seals. Test boundary selection that tries hardest to deceive a reused holdout.

**Gate:** reproducible population-level null simulation report with uncertainty; no positive attestations without registered family/valid holdout; correct failure/abstention on insufficient power. **Commit:** `test(v3): falsify selective-inference and family-error claims`.

## V3-09 — Reproducible witness and fault-containment closure (P2)

**Work:** type-preserving condition minimization on fresh seeds; atomic recovery at every lifecycle boundary; process descendant accounting audit; derived-cache poisoning; schema backward compatibility and signed export.

**Gate:** independent reproducer verifies signed witness, no prior evidence rewritten, restart equivalence, clear accounting-limit warnings. **Commit:** `feat(v3): harden boundary witnesses and recovery`.

## V3-10 — External competitor harness and sealed multi-host results (P1 prerequisite for SOTA)

**Work:** pinned SMAC3 and Optuna TPE adapters on explicitly aligned black-box objective, independent `ruptures` detector-only evaluation, PerfFuzz/AFL++/SlowFuzz only where shared target exists. Run full frozen held-out suite. Store complete results as CI artifacts/research release with SHA-256 hashes; put small summaries/manifests in git.

**Gate:** results executed, not approximated; competitor failures and setup costs retained; fairness auditor approves comparable rows; scoped SOTA claim has at least one real matched external acquisition competitor, or must say external competitiveness unproven. **Commit:** `bench(v3): publish aligned external comparisons and losses`.

## V3-11 — Claim audit, documentation, release readiness (P1)

**Work:** independent adversarial review and scorecard verifier, final status by claim type; remove stale README PR statuses, update architecture diagrams, paper-style methods appendix, commands and environment cards; optional release candidate.

**Gate:** all mandatory tests + CI at exact release SHA, quality gates, independent signed/hashed scorecards, no unresolved severe scientific validity issue, explicit negative results, user approval before merge/release. **Commit:** `docs(v3): finalize evidence-scoped competitive verdict`.

### Fast path vs optional path

- **Core path:** V3-00 → V3-01 → V3-02 → V3-03 → V3-04 → V3-05 → V3-07 → V3-08 → V3-09 → V3-10 → V3-11.
- **Optional:** V3-06 only if simpler policies lose/plateau in a way a learned surrogate plausibly fixes and predeclared evaluation supports it.
- A complex model is **not** a higher-priority milestone than honest benchmarks, legal candidate representation or valid fresh-seed contrasts.

---

# 14. Required test and CI matrix

**Local exact commands** (run, do not claim without output):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
bash scripts/acceptance_smoke.sh
bash scripts/coverage.sh

cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang -j2
ctest --test-dir build-clang --output-on-failure

cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DPHASESHIFT_SANITIZE=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir build-sanitize --output-on-failure
```

Use actual repository scripts and dependencies, not invented wrapper commands. CI:

- GCC and Clang build + all CTests; strict warning gates.
- ASan + UBSan with zero *new* regression.
- Production executable line coverage >=95%, branch coverage reported separately.
- v1/v2 replay/CLI golden contracts and SQLite migrations from known old versions.
- v3 exhaustive small-domain feasibility oracle, deterministic index & scheduler replay.
- End-to-end real-worker mixed-space benchmark with budget cap, crash and invalid statuses.
- Independent full-engine statistical/campaign controls, protected from adaptive leakage.
- Cost/calibration numerical fuzz, malformed input fuzz and old output read-only verification.
- Frozen held-out research as **explicit separate CI/manual-release gates** where expensive. Avoid making every routine PR rerun an enormous campaign; but never count a skipped/reused old held-out result as verification of new algorithms.
- Build artifacts and linked source/plan/scorecard checksums retained with realistic expiration; release evidence needs durable storage beyond 14-day ephemeral CI artifacts if a durable claim is published.

**New or altered code must not “fix” tests by weakening them.** Investigate underlying failure first. If a test is malformed, repair its independently documented truth contract and record the falsification ledger entry.

---

# 15. Research and evidence governance

For every claimed improvement, retain a machine-readable record:

```json
{
  "claim_id": "V3-PRIMARY-AUC-001",
  "claim_type": "EMPIRICAL_BENCHMARK",
  "status": "NOT_YET_TESTED",
  "source_sha": null,
  "baseline_sha": "279a8c65ad7ec3741a727ff49aa7f84e0628a6f1",
  "policy_version": "active_boundary_v3",
  "objective": "true first-confirmation success AUC vs consumed worker CPU",
  "eligible_baselines": ["adaptive_partition", "cliff_bracket_v2", "hybrid_mixed_v2"],
  "training_manifest_sha256": null,
  "heldout_manifest_sha256": null,
  "raw_result_artifact_sha256": null,
  "independent_verifier_version": null,
  "effect_estimate": null,
  "uncertainty_interval": null,
  "null_result": null,
  "failure_or_exclusion_ledger": [],
  "limitations": ["Must align observed CPU consumption", "Requires independent ground truth"]
}
```

**Allowed claim states:** `PROPOSED`, `IMPLEMENTED`, `TESTED`, `REPRODUCED`, `EMPIRICALLY_BETTER`, `SOTA_COMPETITIVE`, `SOTA_SUPPORTED`, `REFUTED`, `BLOCKED`. A SOTA state is **scoped to the benchmark and competitor set** and is never inferred merely from code quality. Each claim's evidence state and potential novelty/prior-art state are separate.

**Artifact rules:** store large per-run `run.sqlite`, logs and exact raw observations outside git in retained versioned artifacts or a repository release with hashes; commit only concise manifests, scripts, scorecards and evidence pointers. Re-run from clean checkout before any public performance claim. Do not expose secrets or embed personally identifying machine paths in public datasets.

---

# 16. Top risks, triggers, mitigations and rollback

| Risk | Observable trigger | Correct response |
|---|---|---|
| Surrogate overfits 1D calibration | loses on rare/XOR/noisy heldout | disable learned policy; keep fixed portfolio |
| Search optimization uses wrong metric | fastest invalid cases are selected | block ranking, preserve failure evidence, fix oracle contract |
| Budget gaming | same 15s ceilings but 0.8s vs 9s spent | switch to consumed-budget curve and saturating workload |
| Overstated calibration | model uses 1–2 samples per structure | label heuristic, refuse probability claim |
| Statistical leakage | confirmation data affects future search or hyperparameter selection | fail campaign, freeze new independent holdout |
| Multiplicity omission | cherry-picked best H1 after many attempts | family registry with all eligible hypotheses, Holm/valid correction |
| Conditional explosion | large factorial space/rare constraints | bounded legal cell/slice screening and explicit `UNSUPPORTED_COMPLEXITY` |
| Heap/caching overhead | overhead > benefit on cheap workers | automatic deterministic v2 fallback, include overhead in scoreboard |
| Source/witness corruption | altered SHA, mismatched cache/checkpoint | fail closed, rebuild only derived state, never rewrite raw |
| Compiler-dependent float divergence | replay transcript differs across platforms | declare supported replay profile and canonical deterministic ordering |
| CI green, real benchmarks weak | tests pass but primary endpoint loses | do not promote v3; publish negative result |
| External comparison unfair | unmatched objectives or invalid program contract | mark `INCOMPARABLE`; do not count as SOTA win |
| Experimental regression | new v3 mode alters old default behavior | version switch rollback and hard regression gate |
| Untrusted plugin execution | third-party native code outside trust boundary | document trusted-only; do not advertise security isolation |
| Stale docs | historical PR status used as current fact | update current status without fabricating past claims |

**Rollback invariant:** old policies remain selectable and original evidence remains loadable; a v3 model can be disabled without schema data destruction.

---

# 17. Definition of done: what “SOTA PhaseShift” would actually mean

## Engineering done

- [ ] v3 features implemented with usable real CLI, not stubs.
- [ ] old v1/v2 semantics and tested fixtures unchanged or appropriately versioned.
- [ ] deterministic legal mixed-space policies with independently checked small-domain truth.
- [ ] robust independent matched-seed discovery measurement and separate sealed confirmation.
- [ ] evidence/transaction/worker faults handled without counterfeit observations.
- [ ] exact-SHA GCC/Clang, sanitizer, CTest, >=95% line coverage, smoke and migration gates green.

## Scientific done

- [ ] fully frozen independent benchmark, genuine ground truth for synthetic cases.
- [ ] budget-saturating and actual consumed-resource comparisons, all failures retained.
- [ ] strong incumbent v2 baselines included; all ablations and losing cases published.
- [ ] meaningful improvement across multiple diverse held-out families, not a single threshold.
- [ ] separate null/statistical calibration with uncertainty and recorded assumptions.
- [ ] independently verified witness and family registry on every promoted claim.
- [ ] matched SMAC3/TPE acquisition benchmark and at least one justified external detector/fuzz comparator as separately scoped evidence, **or candid `NOT_ESTABLISHED`**.
- [ ] repeated-session and, if claimed, multi-host results.
- [ ] independent adversarial reviewer/scorecard verification, reproducible artifacts.

## Public claim allowed only after the above

A defensible statement would be:

> “At source revision `<sha>`, PHASESHIFT v3 achieved `<measured outcome>` relative to `<named baselines>` on `<frozen matched benchmark>`, under `<CPU/wall budget>` and `<statistical protocol>`, while preserving oracle correctness, replay and the specified false-promotion controls. This result does not claim superiority on different objectives or untested hardware.”

If the effect or competitor comparisons fail, **the project can still be excellent engineering**; label the result `EMPIRICALLY_NOT_SUPERIOR` or `SOTA_NOT_ESTABLISHED`, and promote only the best verified policy for its applicable regime. Do not suppress the result.

---

# 18. Immediate execution handoff (first three sessions/PRs)

**PR A — Benchmark truth first.** Freeze new `benchmarks/v3` manifest and compute-consumption scoreboard. Re-run `adaptive_partition`, `cliff_bracket_v2`, `hybrid_mixed_v2`, `stratified_v2` on a hard, budget-saturating multi-scenario control. Publish raw losses. This is the reference to beat.

**PR B — Typed control-plane extraction.** Add stable `CandidateKey`, `DiscoveryEvidenceSnapshot`, `BudgetSnapshot`, `SearchPolicyV3` adapter and slim orchestration around the existing `src/experiment.cpp`, with byte-for-byte v1/v2 characterization tests. No new policy wins are claimed during this refactor.

**PR C — Adaptive seed-matched replication.** Add truly paired discovery runs, contrast noise/cost diagnostics, deterministic resume, and a conditional replication-vs-explore experiment. Run against the frozen benchmark. If this makes things worse, retain the logic as opt-in and document why.

**Then:** conditional region portfolio → robust cost acquisition → multi-boundary taxonomy → campaign-wide falsification → trustworthy external competition → final verdict. Optional surrogate only if the simpler system's measured limitations justify it.

---

# 19. Current prior-art reference map (verified public reference points, 21 Nov 2026)

These are **comparison and mechanism inspirations**, not authority to claim PhaseShift surpasses any of them:

1. **PerfFuzz**, Lemieux et al., *Automatically Generating Pathological Inputs*, ISSTA 2018. Performance feedback-directed fuzzing for pathologically expensive inputs. Original paper: https://people.eecs.berkeley.edu/~ksen/papers/perffuzz.pdf ; implementation: https://github.com/carolemieux/perffuzz . **Scope mismatch:** stress maximization vs boundary confirmation. Compare on a deliberately shared objective only.
2. **SMAC3**, official documentation and code: https://automl.github.io/SMAC3/latest/3_getting_started/ ; https://github.com/automl/smac3 . Random-forest/GP-based black-box algorithm configuration and multi-fidelity search depending on facade. Suitable benchmark-only mixed-variable search baseline after objective alignment.
3. **Optuna TPE**, official API: https://optuna.readthedocs.io/en/stable/reference/samplers/generated/optuna.samplers.TPESampler.html . Conditional and multivariate settings are relevant; optimization goal is not natively boundary localization.
4. **BoTorch cost-aware Bayesian optimization**, official tutorial: https://botorch.org/docs/tutorials/cost_aware_bayesian_optimization . Illustrates expected improvement per cost; not a mandatory dependency or proof that cost-normalized acquisition solves boundary search.
5. **BoTorch acquisition functions**, official: https://botorch.org/docs/acquisition . Supports noisy/constrained acquisition designs, a useful mathematical reference, but a GP/MC acquisition is not automatically affordable in CPU-only C++ search.
6. **Active Learning for Level Set Estimation**, Gotovos et al., IJCAI 2013: https://mlanthology.org/ijcai/2013/gotovos2013ijcai-active/ . Relevant active estimation of threshold boundaries; assumptions/guarantees must be checked before borrowing language.
7. **ruptures**, official docs: https://centre-borelli.github.io/ruptures-docs/ . Off-line changepoint detector; comparison must use frozen **observed** sequences and equivalent cost/penalty objectives.
8. **AFL++**, official: https://github.com/AFLplusplus/AFLplusplus and https://aflplus.plus/docs/fuzzing_in_depth/ . Instrumented fuzzing baseline on compatible stress-input tasks, not direct proof of PhaseShift's boundary-discovery quality.
9. **BOHB / Hyperband**, Falkner et al., ICML 2018: https://ml.informatik.uni-freiburg.de/wp-content/uploads/papers/18-ICML-BOHB.pdf . Relevant only if low/high fidelity inputs preserve useful ordering for discovery.
10. **LLVM benchmark methodology**: https://llvm.org/docs/Benchmarking.html . Practical noise controls and cautions about hardware/frequency/affinity confounds.
11. **ACM SIGSOFT Empirical Standards**: https://www2.sigsoft.org/EmpiricalStandards/docs/standards . Benchmark comparability, repeated experiments, methodology and replicability.
12. **Modern post-selection inference caution:** https://proceedings.neurips.cc/paper_files/paper/2024/hash/6b99cfe8f27a30d013f49a970aacd6e8-Abstract-Conference.html . Adaptive selection and later multiple testing require explicit data/holdout treatment, not nominal test labels alone.

**Novelty check:** Active boundary exploration, multiresolution search, BO/cost-aware acquisition, performance fuzzing, changepoint detection and selective inference have established prior art. PhaseShift's potential contribution is an **integrated, reproducible, budget-accounted, independently confirmed boundary-discovery workflow** and possibly a novel validated hybrid policy if comparative experiments establish it. Do not call that combination a new algorithmic theorem or a universally first system without mechanism-level literature review.

---

# 20. Final standing directive

**Deliver verified code, then evidence.** Preserve the tested v1/v2 engine. Fix fair comparison and sample allocation first. Introduce region-aware mixed-dimensional search, fresh paired replicates, cost and uncertainty calibration, shape-aware multi-boundary discovery and independent family-wide confirmation. Prove compatibility by executable tests and source-hashed evidence. Make the design *capable* of SOTA performance, then let independent experiments decide whether it is actually SOTA. Never merge or declare victory without the user's explicit command and the relevant acceptance gates.

---

# 21. Execution outcome and final verdict (23 November 2026)

This section records what happened when the directive was executed. Every statement points to evidence in `docs/v3/`. Where this section and that evidence disagree, the evidence governs.

**Merge:** PR #6 (`feature/phaseshift-v3-sota-research`) was merged into `main` with explicit owner approval, after this record's own commit also passed CI. The last head before this record was `4a18a00`, where every triggered CI workflow passed: GCC and Clang 123/123, ASan/UBSan 123/123, production line coverage 7386/7724 = 95.62%, v1/v2 historical compatibility, the scientific benchmark and multidim truth.

| Milestone | Outcome |
|---|---|
| V3-00 | Baseline and historical schema 7/8/9 replay gate implemented and CI-verified. No post-merge expensive v2 heldout rerun |
| V3-01 – V3-08 | Implemented as opt-in protocols (schemas 10–19) with falsifying tests. States PARTIAL per `IMPLEMENTATION_LEDGER.md`; no policy promoted |
| V3-06 | Not attempted (optional) |
| V3-09 | Journaled holdout attempts, Ed25519-signed witnesses, scope-preserving minimization with fresh-seed reproduction, live-worker containment and storage-fault matrix. Two real evidence-integrity defects were found and fixed. Limits: no cgroups, no genuine ENOSPC, v1/v2 store not hardened |
| V3-10 | Development comparisons executed and archived. A one-time frozen confirmation (cohort 51001–51020, run 45211496039) found H1 V18 > SMAC3 2.4.1 and H2 V18 > Optuna TPE both **NOT SUPPORTED** (Holm p = 1.0). ruptures PELT parity is detector-only |
| V3-11 | `docs/v3/V3_CLAIM_AUDIT.md` final. The opened V18 sealed internal evaluation showed no superiority |

**Verdict against §17 "Definition of done":**

- The engineering criteria are met for the implemented scope.
- The scientific criteria for a SOTA claim are **not** met. The required "at least one real matched external acquisition competitor" comparison was executed, and it found no advantage.
- PhaseShift v3 is therefore an auditable, falsifiable boundary-discovery engine whose **external competitiveness is unproven**.

**Standing constraints for any future work on `main`:**

- **Burned cohorts.** Cohorts 7101–7120 and 51001–51020 are opened and must never be reused.
- **New claims need a new confirmation.** Any new external claim requires a newly frozen protocol on a new, derived-seed-disjoint cohort, preferably with workloads less saturated than `sort_ascending`.
- **Preserved evidence.** The archived evidence and frozen manifests stay unchanged.
