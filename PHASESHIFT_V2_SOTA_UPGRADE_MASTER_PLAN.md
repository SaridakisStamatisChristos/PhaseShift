# PHASESHIFT v2 — SOTA-Oriented Algorithmic Discovery Engine
## Autonomous Implementation Master Directive and Acceptance Contract

**Document ID:** `PHASESHIFT-V2-IMPLEMENTATION-001`  
**Prepared:** 2026-11-20  
**Target repository:** https://github.com/SaridakisStamatisChristos/PhaseShift  
**Inspected baseline:** `main` at `93b47590c7e44eaf062d97995e9c5cd33274a7a2`  
**Implementation language:** C++20 (offline, CPU-only; Linux x86-64 first)  
**Execution mode:** Autonomous, core-code-first, verified milestone commits, dedicated branch + draft PR  
**Mission:** Increase PHASESHIFT's *measured discovery quality per unit compute* without degrading its v1 scientific evidence, correctness, fault-containment, or reproducibility contracts.

> **Status of this document:** Implementation specification and research plan, **not an implementation, performance result, proof of novelty, or SOTA certification**. The next-session agent must re-read the *live* repository before editing, because `main` may have changed.

---

# 0. ONE-SHOT EXECUTION DIRECTIVE — PASTE/INVOKE IN NEXT SESSION

**Agent instruction:**

> Implement **PHASESHIFT v2** in `https://github.com/SaridakisStamatisChristos/PhaseShift` according to `PHASESHIFT_V2_SOTA_UPGRADE_MASTER_PLAN.md`. Start by inspecting the current `main` commit, `AGENTS.md`, `docs/PHASESHIFT_SPEC.md`, build/test scripts, all relevant core files, and existing CI results. Preserve existing functionality and historical evidence. Create a new working branch `feature/phaseshift-v2-research-engine` (or another clearly named non-conflicting branch). Proceed autonomously, in the milestone order specified below. **Write actual working C++20 code** rather than conceptual stubs. At each milestone run meaningful tests, record evidence, commit a coherent change, push the branch, and keep the draft PR updated. Maintain a running `docs/v2/IMPLEMENTATION_LEDGER.md`. If a candidate fails correctness, reproducibility, or head-to-head performance criteria, repair it or retain the proven v1 fallback; do not game thresholds, cherry-pick results, or fabricate evidence. Do not merge the PR, force-push, rewrite history, introduce a license, or expand into cloud/auth/UI/platform portability unless the repository owner separately requests it. Finish with a precise evidence-backed verdict and prioritized residual gaps.

**User interaction:** This is a self-contained directive. Do not ask the owner to choose algorithms, libraries, milestones, hardware, or default parameters unless blocked by a permission/safety issue that genuinely cannot be resolved by a conservative default. Use the strongest available repository/code/CI tools. Keep progress and artifacts in the repository. Avoid costly repeated rereading once the source revision and handoff ledger have been established.

**Core constraint:** Maintain **v1 compatibility** by default. Every v2 algorithmic behavior must have an explicit version/policy flag so that existing v1 experiments, SQLite data, witness bytes, plugin ABI, replay, and reports remain meaningful.

---

# 1. BASELINE: WHAT EXISTS AND WHY IT IS NOT YET PROVEN SOTA

The inspected repository already has:

- C++20 CLI and worker (`apps/phaseshift.cpp`, `apps/phaseshift_worker.cpp`).
- Built-in sorting, Top-K, shortest-path, and synthetic calibration experiments (`src/algorithms.cpp`).
- Versioned case generation and independently checked correctness oracles (`src/core.cpp`).
- Typed and observed morphology features (`src/morphology.cpp`).
- Grid, random, stratified, and feedback-adaptive exploration (`src/experiment.cpp`).
- Balanced ABBA/BAAB timing, bounded child workers, status-aware censoring, and native trusted-plugin ABI (`src/runner.cpp`).
- Case-aware bootstrap, sign-flip tests, limited FDR policy (`src/statistics.cpp`).
- Empirical scaling change detection and new-size holdout (`src/scaling.cpp`).
- Parameter and exact-byte witness minimization, source witness reproduction, and SQLite-backed replay (`src/minimize.cpp`, `src/reproducer.cpp`, `src/store.cpp`).
- Versioned provenance/evidence, exports, bundled verification, extensive CTest/CI, sanitizer testing, and a 95% **line** coverage gate.

At inspected SHA `93b47590c7`, the `build-test`, `sanitizers`, and `production-line-coverage` workflows all completed successfully. Verify the live commit and workflows before making a new claim. This is strong *implementation-quality* evidence, **not evidence of SOTA discovery performance**.

## 1.1 High-leverage limitations observed in source

1. `grid_cases()` in `src/experiment.cpp` materializes the Cartesian product up to `max_queue`. This prevents exploring very large candidate universes without first materializing them.
2. `adaptive_next()` uses a deterministic exploration quota and a gap/contrast heuristic, scanning the candidate pool and reconstructing observations. It has no calibrated uncertainty model, cost-aware acquisition function, or robust multi-dimensional search policy.
3. Numeric detection in `detect()` is primarily adjacent crossover detection or a single split with segmented mean/BIC and hand-set thresholds. It does not search for multiple independent change points or distinguish broad smooth changes from localized transitions as rigorously as possible.
4. `fit_growth_models()` considers a fixed small library of shapes; `detect_scaling_boundary()` fits one log-log slope change. The valid empirical result is *finite-range model behavior*, not Big-O.
5. `measure_one()` retains timed outputs for later validation; this is correctness-conservative but affects allocations/cache. Currently, measurement protocol choice is limited.
6. Discovery runs may compare structurally neighboring inputs generated under *different* default seeds. Although confirmation does matched-seed re-evaluation, v2 should support controlled-seed discovery comparisons to reduce search noise/confounding.
7. Existing statistical control is strongest for a **single frozen, single-use confirmation**. It does **not** establish global error control across unlimited repeated research campaigns.
8. Current prior-art research and synthetic ablations do not establish superiority over competitive search, changepoint, or performance-fuzzing methods.

**Important:** These are improvement opportunities from code inspection, not claims that v1 is incorrect. Do not re-open already repaired v1 audit findings without an actual reproducer.

---

# 2. DEFINITION OF SUCCESS AND SCIENTIFIC CLAIM BOUNDARY

## 2.1 Primary objective

Improve the probability of finding and independently confirming a *real structural algorithm behavior boundary* under matched **wall-clock and worker CPU budgets**, while maintaining explicit false-positive, replay, and witness constraints.

Optimize the **vector** of outcomes, not one gameable scalar:

1. **Discovery yield**: independently confirmed true boundaries per campaign, where ground truth is available.
2. **Localization quality**: boundary interval width/error and precision/recall for multi-boundary fixtures.
3. **Time-to-confirmation**: wall seconds and actual CPU cost to the *first* valid confirmed boundary.
4. **Coverage/diversity**: number of distinct structural regimes/interaction conditions found, with duplicates deduplicated.
5. **False-positive behavior**: candidate and confirmed discoveries in null/smooth/no-change datasets.
6. **Reliability**: rate of invalid/censored samples, incorrect confirmations, replay mismatch, and worker failures.
7. **Overhead**: parent scheduler/memory/serialization cost and peak RSS.
8. **Reproducibility**: byte-identical generated cases and policy decisions under fixed replay conditions; acceptable scientifically explicit timing variation between hosts.

## 2.2 Success classification (do not conflate)

- `IMPLEMENTED`: feature exists and compiles.
- `TESTED`: adversarial/unit/integration acceptance passed on a recorded revision.
- `REPRODUCED`: results regenerate from frozen seeds, config, and binaries.
- `EMPIRICALLY_BETTER`: outperforms declared baseline on clearly described held-out workloads and equivalent budgets, with uncertainty.
- `SOTA_COMPETITIVE`: measured alongside recognized external methods in a *shared, justified problem regime*.
- `SOTA_SUPPORTED`: breadth and quality of external comparisons support a carefully scoped leading-performance claim.

**No undocumented "SOTA" label or unqualified speedup.** A failed hypothesis is a valid finding and must be preserved.

## 2.3 Non-goals

Do **not** add: GPU/CUDA, LLM/API dependencies, distributed execution, cloud backends, login/auth, web dashboard, full general-purpose compiler, Windows/macOS mandatory qualification, or a complete untrusted-code sandbox. None is required for the PhaseShift v2 core.

---

# 3. NON-NEGOTIABLE INVARIANTS

The next-session implementation must preserve all of the following:

**I1 — Semantic correctness:** Invalid output, wrong answer, crash, timeout, OOM, truncated worker result, mismatched digest, or insufficient samples can never be treated as a valid timing win. Failed observations remain raw evidence.

**I2 — Historical compatibility:** Legacy configs, generator v1/v2/v3 case identity/byte encodings, CLI commands, existing SQLite v7 stores, immutable bundles, valid native plugin ABI v1, and v1 experiment reports must continue working. New policy semantics require explicit versioning and a migration tested on copied fixtures.

**I3 — Independent oracles:** The tested implementation and reference correctness oracle must remain logically independent. The oracle must not learn expected results from the algorithm under test.

**I4 — Discovery/holdout separation:** Candidate selection and tuning may use discovery evidence only. Fresh confirmation cases/seeds/size sets must be frozen before use; never leak holdout outcomes into search or hyperparameter tuning.

**I5 — Immutable positive attestation:** No `CONFIRMED` without persisted, sealed, SQLite-linked holdout sample/case provenance. Existing v7 legacy treatment must be preserved.

**I6 — No hidden statistical guarantees:** Ordinary fixed-sample 99% paired confidence intervals and sign-flip tests remain the default. Sequential optional stopping must not inherit those fixed-sample guarantees unless an actually valid method is implemented and tested.

**I7 — Deterministic logical replay:** Same policy version, config, seed, case index, scheduler transcript, plugin/source fingerprint, and generator version must reproduce exact case identities and decision transcript. Wall-clock performance need not be byte-identical.

**I8 — Worker containment:** Keep bounded IPC, deadlines, Linux resource limits, process-group kill/reap, ABI checking, plugin SHA-256 identity; worker separation is fault containment, not security sandboxing.

**I9 — Monotone evidence:** Never overwrite or reclassify historical raw data. Derived model versions and later invalidations must be separate, traceable events.

**I10 — No benchmark gaming:** Baselines use identical correctness contracts, available compute, candidate families, initial evidence, measurement settings and resource ceilings wherever feasible. Clearly flag any difference that cannot be matched.

**I11 — Core-first:** No peripheral infrastructure effort until the experimental algorithmic core is functional and verified.

**I12 — Accurate coverage claims:** Preserve the >=95% LLVM production **line** coverage CI gate. Report branch coverage separately and do not mislabel line coverage as branch/MC/DC proof.

---

# 4. TARGET ARCHITECTURE

Preserve existing public v1 entry points. Introduce isolated, testable v2 interfaces rather than enlarging `src/experiment.cpp` indefinitely.

## 4.1 Suggested modules (adapt to source architecture after inspecting it)

```text
include/phaseshift/
  search_space.hpp            # typed axes, cardinality, random access/lazy index
  scheduler_v2.hpp            # policy interface, budgets, deterministic state
  evidence_view.hpp           # read-only indexed discovery observations
  acquisition.hpp             # cost/uncertainty/regime objectives
  boundary_v2.hpp             # multiple change points, confidence/shape labels
  interaction.hpp             # mixed-dimension contrasts, conditional slices
  measurement_policy.hpp      # explicit measurement protocol ID and metadata
  campaign.hpp                # multiple-run inference family/evidence registry
  benchmark_suite.hpp         # repeatable benchmark evaluation contract
src/
  search_space.cpp
  scheduler_v2.cpp
  evidence_view.cpp
  acquisition.cpp
  boundary_v2.cpp
  interaction.cpp
  measurement_policy.cpp
  campaign.cpp
  benchmark_suite.cpp
  ...existing v1 files retained...
tests/
  search_space_v2.cpp
  schedule_replay_v2.cpp
  acquisition_oracles.cpp
  changepoint_oracles.cpp
  interaction_oracles.cpp
  statistical_campaigns.cpp
  measurement_protocols.cpp
  performance_regression_v2.cpp
  compatibility_v1_v2.cpp
  benchmark_reproducibility_v2.cpp
configs/v2/
  ...frozen and validated example configurations...
benchmarks/v2/
  ...scenario manifest, runner, summary/report generator...
docs/v2/
  IMPLEMENTATION_LEDGER.md
  SEARCH_AND_DETECTION_CONTRACT.md
  STATISTICAL_PROTOCOL.md
  BENCHMARK_SPEC.md
  BASELINE_RESULTS.md
  VALIDATION_MATRIX.md
  SOTA_COMPARISON.md
```

These are proposed boundaries, not rigid filenames. Prefer clean minimal change over boilerplate proliferation. Make every new nontrivial module independently unit-testable; keep decision logic out of CLI and SQLite glue.

## 4.2 Typed conceptual contracts

Illustrative interfaces (adapt names and internals to existing `Case`, `Config`, `Store` semantics):

```cpp
struct SearchSpace {
    // Typed axes: ordered integer/fractional, enum, boolean, conditional.
    // Compute cardinality with checked overflow; do not materialize product.
    VirtualCaseSpec at(MixedRadixIndex i) const;
    VirtualCaseSpec sample(uint64_t& rng_state) const;
};

struct ObservationView {
    // Read-only summaries built from COMPLETE oracle-valid discovery cases.
    // Parent-side indexed/cached; never mixes holdout or censored data.
    std::optional<CaseSummary> lookup(CaseKey key) const;
    void applyCommittedDecision(const RecordedDecision& d);
};

struct AcquisitionResult {
    CandidateKey candidate;
    double priority;
    double uncertainty;
    double predicted_cost;
    std::string rationale;
};

class SearchPolicy {
public:
    virtual ~SearchPolicy() = default;
    virtual AcquisitionResult propose(const SearchSpace&,
                                      const ObservationView&,
                                      PolicyState&) = 0;
    virtual std::string version() const = 0;
};

struct BoundaryCandidate {
    BoundaryKind kind;
    std::vector<AxisCondition> condition;
    std::vector<BoundaryInterval> intervals;
    double discovery_effect;
    std::string detector_id;
    std::string model_version;
    // Provenance: frozen selection cohort, feature axes and trial family.
};
```

Do **not** change the C plugin ABI merely to support these internally. Define new interfaces in the C++ core with versioned serialization and adapter paths for v1.

---

# 5. MILESTONE EXECUTION ORDER

## M0 — Freeze the baseline and create a falsifiable benchmark contract

**Dependencies:** None. Do first.

**Actions:**

1. Query current repo `main` SHA and any ongoing draft PRs; inspect `AGENTS.md` and original `docs/PHASESHIFT_SPEC.md`. Do not assume the inspected Nov-20 SHA is still head.
2. Verify clean baseline using CMake/CTest, sanitizer workflow equivalent, coverage gate, CLI acceptance. Preserve test totals and toolchain versions.
3. Record main SHA, compiler/stdlib, CPU model, affinity, governor, environment, source/tree hashes, run duration and sample count.
4. Save real baseline experiments for `grid`, `random`, `stratified`, `adaptive_partition` under fixed seed/config/budget and **do not commit huge raw run directories**. Commit small manifests, summary metrics, and retained CI artifacts or links.
5. Add at least one tiny controlled fixture with a known single threshold, one with multiple thresholds, a smooth/no-boundary fixture, a categorical contrast, a rare small-measure interaction, and a correctness/status boundary.
6. Create and initialize `docs/v2/IMPLEMENTATION_LEDGER.md` and a machine-readable `benchmarks/v2/scenarios.json` (or equivalent validated schema).

**Exit criteria:** Baseline independently runnable; existing main green or known preexisting failures isolated; scenarios/gold truth and metrics frozen before implementation begins.

**Commit:** `test(v2): freeze baseline and benchmark contract`

## M1 — Versioned search and evidence contracts (compatibility foundation)

**Actions:**

1. Introduce explicit `search.policy_version` (`v1` default for old configs; `v2` only when requested) and detector/measurement protocol versions. Validate exact allowed keys and types; preserve legacy rejection semantics.
2. Extract read-only discovery summaries from repeated full scans of `Store`. Cache only committed, oracle-valid observations; incremental update after atomic transaction commit.
3. Define an explicit experiment-budget ledger: `candidate_evaluations`, `discovery_case_count`, `holdout_case_count`, `worker_cpu_ns`, `host_wall_ns`, `parent_overhead_ns` (where measured), `invalid_count`, `incomplete_count`, `peak_rss`, remaining budgets.
4. Define `CandidateKey` and canonical typed coordinate encoding. Keep old `Case::id` and generator versions exactly unchanged. New keys may map to old cases but must not silently change legacy identities.
5. Keep v1 scheduler exactly accessible. Add a compatibility adapter instead of reshaping historical experiment records in place.

**Tests:** Decode v1 schemas; known legacy fixtures; exact old case bytes/IDs, old report output, bundle verification, migration round-trip, new unknown config rejection. Compare v1 run behavior with v1 policy after refactor.

**Exit criteria:** Existing tests remain green and old runs replay without reinterpretation; search v2 can consume typed/cached evidence independently.

**Commit:** `refactor(v2): version search policy and index valid evidence`

## M2 — Lazy, typed, mixed-dimensional candidate universe

**Goal:** Handle a potential search universe much larger than RAM without enumerating all combinations.

**Implementation:**

1. Replace the eager-v2 Cartesian product with a **lazy search-space** model: bounded/explicit integer and PPM fractional axes, categorical labels, boolean flags and legal conditional combinations.
2. Implement checked mixed-radix indexing/unranking where the Cartesian design is legal; use overflow-aware cardinality (`__int128` when available, checked fallback) and explicit `CARDINALITY_OVERFLOW` diagnostics rather than wraparound.
3. For conditional/infeasible combinations, avoid generating illegal cases. Implement indexed admissible subspaces or rejection sampling with bounded attempts and a deterministic fallback. Prefer structural constraints over a global blind rejection loop.
4. Support direct seeded random proposals without materializing the grid. Preserve exactly-once deduplication using deterministic canonical candidate keys.
5. Offer multiple sampling modes: uniform over **valid candidates** (where feasible), stratified-by-axis coverage, weighted domain priors, and targeted mutation around already evaluated candidates. Do not falsely call weighted sampling uniform.
6. For *v2* candidates, separate `structural_coordinates` from `replicate_seed`. When evaluating a boundary contrast, use the **same replicate seed on both sides**, subject to generator semantics, and persist this design. Preserve v1 seed semantics untouched.
7. Add a memory-bound local candidate queue with lazy replenishment. Never construct all `N1×N2×...` combinations simply to choose a few.
8. Version policy RNG state and candidate-selection transcripts. Record proposal origin (random/interval/surrogate/mutation/replication), accepted/rejected generation counts, and exact reason.

**Adversarial tests:** Single empty/constant axis; duplicate categorical labels; many axes with huge cardinality; out-of-range PPM; feasible-space exhaustion; invalid combinations; hash collisions simulated; small-radix exhaustive check; random resume/freeze; byte identity; no O(Nproduct) resident allocation.

**Performance acceptance:** Successfully select and execute a few dozen candidates from a **virtual** grid of >=10^9 combinations under a modest specified memory cap, without enumerating the universe. This is a *scale handling* target, not a claim that 10^9 algorithms were searched. Measure peak RSS and startup time vs old eager grid when both are feasible.

**Commit:** `feat(v2): add lazy mixed-space generation and paired seeds`

## M3 — SearchPolicy v2: uncertainty-aware, budget-aware adaptive search

**Goal:** Replace heuristic-only acquisition with a correct, explainable and cheap hybrid approach.

**Required distinct strategies:**

- `legacy_adaptive_v1`: unchanged control.
- `hierarchical_boundary_v2`: preferred default for 1–3 ordered axes; hierarchical intervals/cells with persistent refinement.
- `hybrid_mixed_v2`: ordered-interval search plus categorical/conditional coverage, including interaction screening (M4).
- `uniform_v2` and `stratified_v2`: strong baselines operating over the same lazy universe.
- `surrogate_v2` (experimental, feature-gated): lightweight, reproducible C++ model, **only** if it materially improves held-out tasks. No mandatory Python/ML runtime.

**Suggested priority function (a design hypothesis, not a mathematical result):**

For a candidate region `R`, compute:

- `contrast`: robust difference among neighboring valid case summaries;
- `uncertainty`: robust seed-replicate variability and lack of observations;
- `geometry`: interval width/region undercoverage;
- `interaction`: extra conditional contrast across other axes;
- `cost`: predicted bounded worker CPU + fixed overhead.

One possible normalized score is:

`score(R) = (wC*C + wU*U + wG*G + wI*I) / max(epsilon, predicted_cost(R))^alpha`

with dimensionless, bounded feature transforms and predeclared weights. **Never claim uncertainty is calibrated merely because it is a variance heuristic.** Optimize/tune weights on discovery/training scenarios, then freeze before a separate evaluation suite. Add a deliberately simple default configuration if tuning introduces instability.

**Scheduler behavior:**

1. Start with a low-discrepancy/stratified warmup where applicable; ensure boundary endpoints/coverage of categorical conditions.
2. Maintain `IntervalIndex`/priority queue and update only affected neighbors after a committed evaluation. Avoid scanning the full candidate universe for each proposal.
3. Enforce an exact deterministic exploration quota (v1's existing rule is a compatibility reference). Preserve true random exploration and systematically cover unsampled categories.
4. Add adaptive *replication*: when uncertain whether an observed contrast is real, spend more discovery budget on **new paired seeds** at those coordinates, not just more coordinate points.
5. Avoid duplicating old candidate/seed pairs. Distinguish a new structural coordinate from a new independent replicate of that coordinate.
6. Bound computational overhead per proposal; if model/queue overhead starts dominating experiments, fall back to simpler policies.
7. Account for observed failed-worker costs without converting censored statuses into performance scores.
8. Preserve deterministic state transitions and persistent transcripts under crash/resume.

**Testing:** Seeded golden schedules; restart equivalence; random exploration quota exactly met; single-bin boundary; plateau; two distant cliffs; noisy false contrast; monotone smooth trends; categorical no-order semantics; concurrent-independent candidate generation not affecting commit order; budget exhaustion preserving resume.

**Exit criteria:** On predeclared held-out synthetic scenarios, no correctness or false-positive regression. Report target accuracy, CPU costs, and scheduler overhead even if v2 loses. Do not make v2 default across all users until results support it.

**Commit:** `feat(v2): add cost-aware hierarchical adaptive scheduler`

## M4 — Multidimensional interactions and conditional boundary localization

**Goal:** Find a transition that appears **only when multiple structural properties interact**.

**Implementation:**

1. Create a typed `Condition` representing a conjunction of numeric ranges and categorical equalities. Do not turn nominal labels into artificial numbers.
2. Perform low-cost main-effect screening (balanced valid samples) plus targeted two-way interaction checks. Pair on replicate seed wherever possible; freeze nuisance dimensions.
3. Use hierarchical conditional partitioning, not an uncontrolled exponential search. Limit interaction order (`2` initially, configurable), depth, minimum independent seed pairs, and number of retained hypotheses.
4. Merge geometrically equivalent candidate boundaries across overlapping cells; preserve their distinct provenance and discovery scores.
5. Add `CONDITIONAL_BOUNDARY`, `MULTIPLE_BOUNDARIES`, `SMOOTH_TREND`, `NO_BOUNDARY` and `INSUFFICIENT_EVIDENCE` semantic categories where scientifically justified. Do not label every strong effect a sharp cliff.
6. Permit a boundary to apply only under a conditioned morphology/algorithm family. Report the exact frozen condition and observed features.
7. Maintain separate methods for ordinal, categorical, graph and discrete structural axes; graph constraints and degree/density coupling must be validated rather than pretending independent free variables.

**Fixtures:** XOR-like two-axis performance interaction with zero marginal effects; a conditionally triggered crash; a categorical×numeric cliff; graph density×weight-class transition; correlated nuisance axes; non-identifiable interaction; 3+ change-point condition.

**Exit criteria:** Find the planted interaction under predeclared budgets in representative controlled cases, without claiming discovery where design is non-identifiable. Preserve v1 single-axis CLI reporting and default behavior.

**Commit:** `feat(v2): detect conditional multi-axis regime boundaries`

## M5 — Boundary Intelligence v2: multiple changes, robust models and change taxonomy

**Goal:** More reliable localization than a single threshold split.

**Implement:**

1. Keep existing `detect()` as `boundary_v1` for compatibility.
2. Add a piecewise-constant **multiple-change-point** detector using dynamic programming with prefix-statistics and an explicit minimum segment size; report complexity and protect memory/time limits. For supported squared-error additive costs, implement a separately tested **PELT** alternative when its pruning assumptions are satisfied; otherwise use the correct exact fallback. Never claim universally linear complexity for PELT.
3. Add piecewise-linear log-log regression for finite-range scaling candidate generation, using numerically stable centered sums and robust alternatives where justified. Cap model degree; reject ill-conditioned/small-sample fits.
4. Distinguish `CROSSOVER`, `PERFORMANCE_CLIFF`, `MULTI_CLIFF`, `REGIME_SLOPE_CHANGE`, `SMOOTH_GRADIENT`, `CORRECTNESS_BOUNDARY`, `RESOURCE_BOUNDARY`, and ambiguous/no-signal cases, with explicit classification criteria.
5. Never treat a sampled timeout or OOM as a huge runtime. Resource-status boundaries are categorical/censored with their own inference and confirmation path.
6. Use candidate-local bootstrap or other validated uncertainty estimates **for discovery ranking only**; independent confirmation remains separate.
7. Expose ranking/filtering policy for top-K candidate hypotheses. Do not silently promote all discovery candidates; declare the confirmation multiplicity family and freeze it.
8. Produce confidence/provisional localization interval and model score under the stated data model; do not call it a frequentist confidence interval unless the method has the necessary coverage support.
9. Require independent oracle-valid samples in each proposed segment, with configurable minimum counts.
10. Preserve scaling holdout's fresh sizes and full invalid-case quarantine. The current fixed model library may remain as a baseline to challenge.

**Tests:** Exhaustive tiny sequences against brute-force partitions; known 0/1/2/3 transitions; short segments; repeated x; outliers/heavy tails; smooth sigmoid without discontinuity; heteroscedastic noise; numerical extremes/NaN/Inf; tied minima; candidate with no holdout space; correct PELT-versus-DP agreement on valid inputs.

**Exit criteria:** Objective zero-error matching on noiseless ground-truth cases; quantified recall/localization on noisy held-out cases; null false-positive limits documented. No asymptotic or causal inference promoted from finite-size fits.

**Commit:** `feat(v2): add robust multi-change-point boundary models`

## M6 — Measurement engine v2, with protocol separation

**Goal:** Improve measurement precision *without changing the meaning of v1 timing evidence*.

**Modes:**

- `legacy_retained_outputs_v1` — exactly preserve historical behavior and retention cap.
- `calibrated_batch_v2` — new explicit protocol using pilot calibration, preallocation where semantically valid, and timestamped worker records; verify all outputs after timed region. Clearly document which result-copy/allocation costs remain inside the metric.
- `kernel_only_v2` — **optional** for an explicit plugin/in-place algorithm interface with exactly defined output and lifetime; never pretend existing allocating algorithms are kernel-only through unsafe subtraction.

**Actions:**

1. Freeze the executable timing contract per mode in `docs/v2/MEASUREMENT_PROTOCOL.md`.
2. Match A/B inputs, warmups, compiler flags, source revisions, affinity, process isolation, and batch calibration. Preserve randomized balanced execution orders.
3. Separate immutable measurement results from postprocessing; make timer/noise statistics descriptive rather than presuming normality.
4. Add drift diagnostics: first/last block contrast, run-order effect, median absolute deviation, frequency/affinity/governor metadata when accessible. Flag nonstationarity and **abstain** when evidence is inadequate.
5. Use hardware counters (cycles/instructions/cache) only as optional **real** `perf_event_open` measurements when permitted. Never infer unavailable counters, and do not make CI require privileged access.
6. Account for measured worker CPU time, parent wall time, sampling/reaping overhead and pilot calibrations. Respect actual worker timeouts and global deadline.
7. All status/answer validation remains mandatory; no destructive optimizer hints that could invalidate work.
8. Test under controlled synthetic timer and known operation-count fixtures before using real timing evidence.

**Tests:** Fast no-op near resolution, large output buffers, zero/short output, allocation-heavy sort, crash/timeout/partial batch, cache-sensitive workload, ASan behavior, signed zero/duplicate Top-K, correct worker kill/reap under deadline.

**Exit criteria:** v1 historical timing is unaffected; v2 produces fully labelled data; any advertised precision gain is based on measured variance/repeatability, not prettier numbers.

**Commit:** `feat(v2): version calibrated measurement protocols`

## M7 — Statistics and multi-hypothesis research campaigns

**Goal:** Scientific reliability when a campaign proposes several candidates.

**Minimum viable implementation:**

1. Preserve the v1 one-frozen-hypothesis fixed-holdout pipeline unchanged.
2. Introduce a campaign manifest that predeclares the maximum number `K` of hypotheses to be confirmed, control type, primary statistic, practical-effect threshold, confidence level, and confirm-seed budget.
3. Reserve and freeze *distinct* discovery/confirmation seed streams and sizes. Make shared-control reuse explicit; if correlations matter, adjust multiplicity assumptions rather than pretending independent trials.
4. For multiple confirmatory hypotheses use a validity-preserving **Holm FWER** procedure as a straightforward default; BH-FDR only when its assumptions are justified and documented; consider Benjamini–Yekutieli under arbitrary dependence if power permits.
5. Make selection rule, family membership, p-values, adjusted thresholds, and both positive and negative outcomes immutable campaign evidence.
6. Keep confirmation fixed sample size by default. If implementing anytime-valid methods/alpha spending, first write the precise method, assumptions and stopping rule, then adversarially simulate null coverage under optional stopping; do not apply ordinary fixed-sample bootstrap at arbitrary stop times.
7. Use hierarchical null/synthetic campaigns to measure family-wise false-positive rates; include false discovery when correlated axes and CPU drift mimic a boundary.
8. Preserve independent-size scaling confirmation and treat multi-regime claims as multiple hypotheses where appropriate.

**Tests:** Hypothesis-selection leakage; rerun the same confirmation twice (must not upgrade stale evidence); identical/mislabelled held-out CaseIDs; partial holdout; multiple-testing threshold examples; high-correlation null; optional-stopping adversary; malformed campaign manifest; numerical p-value edge cases.

**Exit criteria:** Evidence can establish **which** family was controlled and **under what assumptions**; no unqualified global FDR claims.

**Commit:** `feat(v2): add predeclared multi-hypothesis campaigns`

## M8 — Evidence-preserving witness minimization and stronger falsification

**Goal:** Produce small, trustworthy examples of newly discovered conditional/multiple boundaries.

**Implementation:**

1. Keep existing version-3 literal-byte ddmin and legacy parameter shrinking.
2. Define witness predicate per kind: correctness/status must reoccur exactly; performance must reproduce a frozen sign/effect threshold with new paired seed cohorts; conditional boundary must retain its distinguishing condition or honestly narrow the claim.
3. Use multi-stage minimization (parameter simplification -> typed structure removal -> byte-level ddmin -> value simplification). Every candidate transformation must preserve valid case encoding, oracle requirements and graph reciprocity/renumbering rules.
4. Prioritize reducing witness complexity and maintaining causal-contrast *observability*, not just minimizing bytes. Log failed attempts and rejection causes.
5. For noisy performance witnesses allow `PROVISIONAL_MINIMIZATION`, `MINIMIZED_CONFIRMED`, or `NOT_REDUCIBLE` with explicit checks; never promise global minimality from heuristic ddmin.
6. Revalidate on sealed independent cases and include standalone C++20 reproducer only where existing supported built-in algorithm semantics permit.
7. Add typed cases where structures are small but the performance gap disappears: reject those proposals.

**Exit criteria:** Every newly reduced positive witness is independently rechecked; native plugin binaries remain fingerprinted; v1 replay/export/import remains valid.

**Commit:** `feat(v2): strengthen conditional witness minimization`

## M9 — Scalable persistence and exact replay

**Goal:** Make v2 experiments robust across interruption and long runs without corrupting historical evidence.

**Implementation:**

1. Add an incremental SQLite migration (starting from current schema v7, as verified live). Never rewrite old rows to reinterpret v1 findings.
2. Proposed new tables/fields: `policy_versions`, `proposal_decisions`, `candidate_keys`, `replicate_cohorts`, `acquisition_scores`, `boundary_models`, `candidate_rankings`, `campaigns`, `campaign_members`, `budget_ledger`, `measurement_protocols`, `benchmark_manifest_ref`.
3. All new evidence links to experiment IDs, CaseIDs, SampleIDs and algorithm fingerprints with foreign keys and uniqueness constraints.
4. Commit proposal/decision, measurements, budget deltas, and RNG state atomically at defined transaction boundaries. A crash between worker execution and commit must never double-count data or claim a completed case.
5. Resume exactly from persisted state and deterministic policy version. If model recomputation is used instead of serialization, ensure exact reproducible floating-point ordering/rounding on supported environment, or store canonical model state.
6. Keep bundle integrity, report derivation, import/export and tamper detection. Add explicit v2 compatibility tests including v7->v8+ migrations and read-only old report projections.
7. Index common scans; bound transaction duration and in-memory cache. Do not introduce an external database requirement.

**Failure injection:** Kill process before/after scheduler record; power-loss-style truncated transactions; unknown schema; old schema migration; blob tamper; changed plugin; duplicate CaseID; altered algorithm; large candidate cardinality; SQLite busy/corruption errors.

**Exit criteria:** Crash-resume and uninterrupted v2 sessions produce the **same logical sequence**, set of complete samples, candidate IDs and final decision/evidence state for deterministic fixtures. Instrumented wall times may differ.

**Commit:** `feat(v2): persist campaigns, policies and exact restart state`

## M10 — Competitive benchmark suite: internal baselines and external references

**Goal:** Answer whether v2 is actually better, where, and by how much.

### 10.1 Benchmark families

**S0 Synthetic gold-standard controls:** One cliff, multiple cliffs, crossover, smooth transition, monotone without cliff, no-change noise, heteroscedastic noise, isolated outlier, rare region, skewed cost, categorical-only and XOR interaction. Each has planted ground truth. These test detection/localization/calibration without claiming a real-world discovery.

**S1 Sorting:** `std_sort`, `mergesort`, first-pivot quicksort, insertion sort; asc/desc/organ-pipe/Zipf/heavy duplicates/near-sorted, varied size/uniqueness/disorder. Correctness gate mandatory.

**S2 Top-K:** full sort, heap and nth-element at varied `k/n`, duplicates, ties, nearly equal values; including regimes where method ordering changes.

**S3 Graph:** heap/dense Dijkstra, 0–1 BFS, Bellman-Ford where valid, varying vertices, density, weights, degree skew and components. Respect algorithm restrictions and semantic equivalence.

**S4 Stress/status:** Correctness failures and resource/timeout boundaries in a controlled, declared fixture, not forged OOM data.

**S5 External controlled workloads:** A small set of truly external C++ algorithm implementations wrapped behind the current valid plugin contract or built-in independent oracles. Record exact source release/license and semantic contract. No invented competitors.

### 10.2 Internal search baselines

- Original `grid`, `random`, `stratified`, `adaptive_partition` with v1 semantics where comparable.
- New `uniform_v2`, `stratified_v2`, and low-cost hierarchical policy.
- Ablations for **no-uncertainty**, **no-cost**, **no-replication**, **no-interaction**, **no-random-exploration**, **single- vs multi-change detector**.
- Include naive binary search only for valid monotonic single-boundary instances; it is not a general multi-dimensional baseline.

### 10.3 External comparisons (only in scientifically matched regimes)

- **PerfFuzz**: performance-guided fuzzing/pathological-input discovery. Compare on suitable instrumented programs and common stress objectives; do **not** compare its worst-case maximization directly to a PhaseShift boundary-localization error and call either a win.
- **SlowFuzz**: automated algorithmic-complexity stress-input search. Same caveat.
- **PerfGen**: performance benchmark generation for big-data analytics, where reproducible common targets can be built at reasonable local cost.
- **SMAC3**: strong algorithm-configuration/mixed-variable model-based search, only when an adapter and matching black-box objective can be defined. Since v2 is standalone C++, a *benchmark-only* external Python baseline is acceptable but must not become a runtime dependency.
- **PELT/ruptures**: detection-only benchmark on the *same frozen observational sequences*. This isolates changepoint quality from candidate-generation differences.

If an external tool is infeasible, explain exact prerequisites or semantic mismatch. Never report “beats PerfFuzz/SMAC3” merely because the tool was not run.

### 10.4 Frozen experimental protocol

1. Pre-register workload families, source versions, ground-truth boundaries, outcome metrics, wall/CPU/measurement budgets, seeds, exclusions and failure criteria.
2. Use a **training/development set** to select heuristic parameters; a **sealed held-out evaluation set** to report results.
3. Repeat each main controlled scenario over enough independent seeds (suggested development: 5; final: >=20 if budgets allow). For real measurements, repeat on more than one session and quantify noise/drift.
4. Equal **successful case count** comparison **and** equal **actual wall/worker CPU time** comparison are separate scorecards. Report failures and pilot/calibration overhead; do not claim equal budgets solely by equal case counts.
5. Use the same machine, compiler, core affinity, governor if controllable, timeouts, case generator families and correctness oracles. Randomize policy order within repeats to reduce host drift bias.
6. Report discovery precision/recall, localization width/error, time to first confirmed signal, false-positive rate, confirmation success, CPU seconds, wall seconds, memory, and parent overhead. Mark metrics N/A where ground truth unavailable.
7. Use paired scenario/seed differences and bootstrap intervals for **descriptive cross-policy comparisons**, plus predeclared inferential tests if making statistical claims. Be careful: timing runs across a single machine are not automatically independent hardware replicates.
8. Provide per-scenario and aggregated performance without hiding losses. Include geometric means only where the ratio metric is defined; include absolute medians and spread.
9. Require reproducible automation from clean checkout. Commit concise CSV/JSON/Markdown summary and checksums, not huge raw evidence blobs; store complete records as artifacts when available.
10. Require a separate adversarial “falsification suite” designed to make v2 worse (rare late cliffs, high noise, categorical imbalance, many dimensions, different per-candidate costs, no boundary).

### 10.5 Promotion thresholds (predeclared engineering targets, not promised outcomes)

- **Hard safety gate:** 0 newly introduced wrong-answer promotions, missing holdout attestation, corrupted legacy evidence or replay mismatches on the defined test suite.
- **Hard false-positive gate:** No statistically convincing increase in held-out **confirmed** false positives versus v1; quantify uncertainty, particularly for null-family cases. If sample size is too small, report *not yet established* rather than passing by assumption.
- **Performance target A:** >=20% improvement in median *time-to-correctly-confirmed boundary* against best comparable v1 search baseline across predeclared planted-boundary tasks, with no catastrophic degradation hidden by the median.
- **Performance target B:** >=15% improvement in mean/median boundary-localization error or >=15% more detected ground-truth boundaries under the same budget where metrics are valid.
- **Practicality target:** Scheduler overhead remains within 10% of total wall time for representative nontrivial workloads, or document why a higher fraction materially improves discovery results.
- **Reliability target:** End-to-end experiments reproduce all logical decisions after restart; >=95% line coverage gate remains green.

**Crucial:** These are target hypotheses; if they fail, do not report a pass, do not distort data, and do not force v2 as default. Preserve best-performing validated policy per domain, and publish negative findings.

**Commit:** `bench(v2): add frozen SOTA challenge and comparative results`

## M11 — End-to-end CI, fuzzing, and adversarial validation

**Commands must actually run** (adapt for tool availability and all new tests):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
bash scripts/acceptance_smoke.sh
bash scripts/coverage.sh

cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang -j2
ctest --test-dir build-clang --output-on-failure

cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPHASESHIFT_SANITIZE=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure
```

Keep the original CLI surface (`list`, `validate`, `demo`, `explore`, `resume`, `confirm`, `minimize`, `reproduce`, `replay`, `report`, `scaling`, `ablate`, `intervene`, `export`, `verify-bundle`, `import`, `compare`). Add a v2 CLI flag/config surface only where actually useful.

**Testing tiers:**

- Unit: each sampler, score normalization, checked radix arithmetic, detector, min segment, statistical adjustment, invariants.
- Differential/property: tiny exhaustive state-space, v1 policy preservation, brute-force change-point oracles, fixed vs resumed scheduler.
- Integration: worker/SQLite/CLI including new policy and reports; plugin boundary/failure handling.
- Adversarial: corrupted config, near-overflow dimensions, malformed IPC, missing plugin, abrupt kill, fake holdout labels, duplicate samples, resource failure.
- Scientific null: engineered negative controls and real no-boundary families. No hidden relaxation if a null fails.
- Performance: stable predeclared regression set with disclosure of measurement noise; performance regressions must be investigated, not suppressed.
- Coverage: all new source translation units instrumented; >=95% executable line coverage over `src/` + `apps/`; branch coverage reported as an ungated descriptive number.

**CI:** Preserve GCC/Clang, sanitizer, 95% coverage workflow and CLI smoke; add a lightweight deterministic v2 acceptance gate. Heavy statistical benchmark campaigns may run in explicitly dispatched CI or local reproducible scripts, **not** on every PR if too slow. Pin workflow dependencies and provenance when feasible; existing actions should not be changed merely to obtain a green check.

**Exit criteria:** Exact target revision has passed local tests and GitHub-hosted CI. Any required runner/tool unavailable is reported as pending, not passed.

**Commit:** `test(v2): close correctness reproducibility and CI gates`

## M12 — Documentation, release candidate, and evidence-backed SOTA verdict

**Deliver:**

1. `docs/v2/SEARCH_AND_DETECTION_CONTRACT.md`: problem model, invariants, algorithms, worst-case complexity, memory footprint, fallback rules, known limitations.
2. `docs/v2/STATISTICAL_PROTOCOL.md`: selection family, holdout design, effect thresholds, multiplicity and optional-stopping restrictions.
3. `docs/v2/BENCHMARK_SPEC.md` + `BASELINE_RESULTS.md`: full pre-registration and preserved baselines.
4. `docs/v2/SOTA_COMPARISON.md`: comparative table with honest scope, references, exact configurations and negative findings.
5. `docs/v2/VALIDATION_MATRIX.md`: feature-by-feature source/test/evidence/CI links with states `IMPLEMENTED`, `LOCALLY_VERIFIED`, `CI_VERIFIED`, `EMPIRICALLY_VALIDATED`, `UNVERIFIED`.
6. `docs/v2/IMPLEMENTATION_LEDGER.md`: milestone commit SHAs, PR URL, actual tests, core benchmarks and residual issues.
7. Config examples and CLI usage for v1 default and explicit v2 opt-in, plus compatibility/migration notes.
8. Concise release-candidate summary and targeted future backlog. Do not choose a license or tag a release without authorization.

**Exact final agent response format:**

```text
PHASESHIFT V2 — FINAL IMPLEMENTATION VERDICT
Main inspected: <SHA>
Implementation branch: <name>
Latest branch commit: <SHA>
Draft PR: <URL or not created + reason>
Completed milestones: <M0..M12>
Unfinished milestones: <reason/evidence>
Core source changes: <files/components>
GCC/Clang/ASan/UBSan: <test counts and real result>
Line/branch coverage: <actual percentages, distinguish>
Legacy v1 compatibility: <evidence>
Holdout/attestation correctness: <evidence>
Best comparable v1 baseline: <name/result>
V2 head-to-head results: <absolute + relative + uncertainty>
False positives / negative controls: <measured>
External SOTA comparisons: <executed/not executed and why>
Claims supported: <specific scope only>
Residual issues: <highest priority>
Merge status: NOT MERGED (unless separately authorized)
```

**Commit:** `docs(v2): publish final verification and benchmark evidence`

---

# 6. IMPLEMENTATION DETAILS THAT ARE EASY TO GET WRONG

## 6.1 Paired structural cases are not duplicate cases

The conceptual object under study is often `(algorithm, morphology coordinate, seed)`, not just `(algorithm, coordinate)`. For boundary search, comparing `n=100` at seed A against `n=101` at seed B adds input randomness. V2 should support coherent *paired* designs `(x_left, seed_i)` vs `(x_right, seed_i)` while ensuring `CaseID` remains unique per full input and stale legacy data cannot be reinterpreted. Paired seeds are not guaranteed identical byte-level nuisance features at different structural sizes; record **observed** morphology and residual differences and abstain from strong causal attribution.

## 6.2 Avoid an unearned Gaussian-process SOTA label

A Gaussian process, random forest, TPE, or other surrogate may be a good benchmark or research candidate. Adding one does not automatically improve boundary discovery or establish novelty. First ship interval/interaction search with transparent controls, measure the overhead, and only promote `surrogate_v2` if its held-out outcome is better. If a model cannot be implemented rigorously under the C++/offline constraints, leave it experimental—not a mandatory dependency.

## 6.3 Distinguish objective functions

`max_runtime` (SlowFuzz-like), `max_branch_count` (PerfFuzz-like), `boundary_localization`, `crossover_detection`, `multi-change_detection`, and `failure_case_minimization` are **different objectives**. External tools should be compared only on appropriate shared tasks and semantics. Report Pareto tradeoffs or separate leaderboards rather than a fake single universal score.

## 6.4 Avoid selection and optional-stopping leakage

Repeatedly examining the same confirmatory holdout until the p-value looks good invalidates the default fixed-sample significance interpretation. Hyperparameter tuning on the held-out benchmark also invalidates independent evaluation. Predeclare confirmation, seal it, and start a genuinely new protocol/campaign if revisiting the hypothesis. If an anytime-valid route is added, test its actual assumptions and finite-sample behavior.

## 6.5 A large virtual grid is not a large completed experiment

Virtual cardinality, discovered valid candidate count, unique generated cases, worker-executed cases, oracle-valid measured cases, and sealed independent confirmation cases must be separately reported. A trillion-point virtual search is useful only if the sampler efficiently proposes relevant cases without enumeration.

## 6.6 Complex models can lose to simple search

Many real sorting/graph tasks have low-dimensional discrete structural spaces. Hierarchical search, stratification, and targeted paired replication may outperform expensive surrogate inference. Use adaptive strategy selection under measured overhead, not buzzword accumulation.

## 6.7 Compatibility and experimental-mode distinction

`v1` historical reports and `v2` policies/measurement protocols must remain separable. A new wall-time protocol is a new measurement construct; do not draw numerical trend lines across incompatible protocols without calibration and caveats. New evidence schema is *additive* and uses explicit policy/protocol IDs.

---

# 7. FUNCTIONAL ACCEPTANCE MATRIX (MUST FILL WITH ACTUAL EVIDENCE)

| Gate | Required behavior | Evidence source | Pass rule |
|---|---|---|---|
| G0 | Baseline main/branch traceability | GitHub SHA and ledger | Source/revision pinned |
| G1 | Legacy v1 replay and algorithms | Existing CTest + golden cases | No changed logical results/case IDs |
| G2 | Lazy huge search | Memory/time/carinality tests | No eager combinatorial materialization |
| G3 | Deterministic resume | Kill/resume differential | Same logical decisions/evidence |
| G4 | Search policy correctness | Exhaustive tiny cases + nulls | No duplicates, budget or quota violations |
| G5 | Multiple boundary detector | Brute-force oracle and held-out fixtures | Exact noiseless results; honest noisy uncertainty |
| G6 | Conditional interaction | Controlled 2-way planted cases | Correct condition and no false causal claims |
| G7 | Statistical holdout | SQL/CaseID linkage, null simulations | No unsealed confirmed state or leakage |
| G8 | Measurement protocols | C++ worker and timers | Status/semantic validity; labelled timing |
| G9 | Minimization | Independent witness recheck | No false minimality or wrong-answer promotion |
| G10 | GitHub CI | GCC/Clang/ASan/UBSan/smoke | Exact branch SHA passes |
| G11 | Coverage | LLVM raw report | >=95% production line coverage |
| G12 | Benchmark fairness | Frozen manifests + cost ledger | Comparable budgets and candid differences |
| G13 | Research uplift | Paired benchmark results | Meets declared targets **or** reports failed hypothesis |
| G14 | External comparisons | Actual executed competitor runs | Scoped conclusions only; otherwise pending |

**Release decision:** G0–G12 are hard acceptance obligations when they apply to implemented features. G13/G14 govern the **strength of the performance/SOTA claim**, not whether the underlying correct v2 experimental engine may be retained. Avoid tuning tests until they pass; repair source or narrow claims instead.

---

# 8. MINIMUM VERIFIABLE FIRST SLICE (IN CASE THE NEXT SESSION HAS LIMITED RESOURCES)

If execution resources/time are limited, prioritize the smallest slice with measurable value and do **not** claim a complete v2:

1. Baseline + frozen synthetic evaluation; separate development/holdout fixtures (**M0**).
2. Typed lazy search-space generator with exact restart and old case identity preserved (**M1–M2**).
3. Hierarchical interval priority queue with paired-seed replication, deterministic exploration, cached evidence, and configurable v2 policy (**M3**).
4. One multi-change-point detector with brute-force differential tests and a clean v1 fallback (**part of M5**).
5. Executed v1-vs-v2 equal-budget ablation, CI, and ledger (**parts of M10–M11**).

Then continue M4/M6/M7/M8/M9/M12 after the core demonstrates real value. **Do not** abandon useful working code to produce only architecture documents; source and executable evidence take priority.

---

# 9. RISK REGISTER AND FALSIFICATION CHECKLIST

| Risk | Early warning | Correct response |
|---|---|---|
| New adaptive search only wins one fixture | Losses on held-out rare/smooth/categorical cases | Domain-specific fallback; no universal claim |
| Surrogate model overhead dominates | Parent CPU > time to execute cases | Cache/disable surrogate; simple policy fallback |
| Interaction space explodes | Queue/cardinality/RSS growth with dimensions | Cap interaction order, bounded conditional search |
| Seed pairing changes case semantics | Case digest mismatch/replay failures | New policy/case version; leave v1 untouched |
| Model overfits artificial thresholds | No gain on real algorithms, high null FP | Freeze tuning; stronger realistic benchmark suite |
| PELT pruning invalid for chosen loss | Disagreement with exact small DP | Guard model assumptions; exact fallback |
| Sequential inference is invalid | Optional-stopping false positive inflation | Fixed holdout or validated anytime-valid method |
| Counterfactual interpretation overstated | Morphology features change despite seed match | Report observed confounding and abstain on causality |
| Worker runtime protocol shifts results | Baseline and new mode incomparable | Separate measurement protocol IDs |
| New SQLite schema breaks replay | Old bundle or DB cannot verify | Additive migration/rollback and golden fixtures |
| Hardware drift yields fake transitions | Order/time dependent apparent effects | Randomized order, controls, drift flag/abstention |
| External competitor has different objective | Appears to lose on unrelated metric | Separate task/leaderboard; no false ranking |
| CI passes on old commit only | Checks attached to wrong SHA | Exact-revision verification before release claim |

---

# 10. SOURCE AND PRIOR-ART REFERENCE MAP

These are study/comparison references, not dependencies or claims of superiority. Read current versions before making literature claims.

**PHASESHIFT source (as inspected 2026-11-20)**

- Repository: https://github.com/SaridakisStamatisChristos/PhaseShift
- Original v1 contract: `docs/PHASESHIFT_SPEC.md`
- Repository continuation rules: `AGENTS.md`
- Search/detection: `src/experiment.cpp`
- Scaling: `src/scaling.cpp`
- Statistics: `src/statistics.cpp`
- Timing: `src/runner.cpp`; `docs/benchmark_protocol.md`
- Morphology: `src/morphology.cpp`
- Minimization/replay: `src/minimize.cpp`, `src/reproducer.cpp`, `src/store.cpp`
- v1 evidence: `docs/v1_requirements_evidence.md`, `docs/fresh_audit_repair_2026-11-20.md`, `docs/coverage.md`

**Mechanism-level external references**

- **PerfFuzz**: https://github.com/carolemieux/perffuzz ; ISSTA 2018 performance-guided input generation.
- **SlowFuzz**: https://arxiv.org/abs/1708.08437 ; automated complexity-stressing inputs.
- **PerfGen**: https://doi.org/10.1145/3803437.3806095 ; 2026 work on automated performance benchmark generation for big-data analytics.
- **SMAC3**: https://automl.github.io/SMAC3/latest/ ; mixed-variable model-based algorithm configuration / multi-fidelity optimization.
- **PELT / ruptures**: https://centre-borelli.github.io/ruptures-docs/user-guide/detection/pelt/ ; changepoint-detection mechanism and assumptions.
- **Always-valid inference**: https://pubsonline.informs.org/doi/10.1287/opre.2021.2135 ; repeated monitoring and optional-stopping limitations.
- **IOHprofiler / IOHexperimenter**: https://iohprofiler.github.io/ ; reproducible benchmarking of iterative optimization algorithms.
- **LLVM benchmarking considerations**: https://llvm.org/docs/Benchmarking.html ; careful performance methodology.

---

# 11. FINAL DIRECTIVE

**Build a demonstrably better research engine—not a larger codebase.** Preserve PhaseShift's strongest existing contribution: reproducible, correctness-gated, independently confirmed structural discoveries. Spend implementation effort first on lazy search, matched-seed acquisition, uncertainty/cost-aware scheduling, and better boundary detection. Use adversarial cases to try to defeat every proposed improvement. Only ship new policies as defaults where predeclared held-out evidence justifies them. Create atomic verified commits and preserve every negative result. At completion, provide the exact branch/PR, source changes, tests, baseline comparisons, limitations and a precisely scoped SOTA verdict.

**Authorization boundary:** This document authorizes the *next session to implement on a feature branch when the user invokes it*. It does **not** authorize an automatic merge, repository license change, release publication, or deceptive benchmark claims.