# PHASESHIFT — Full Implementation and Autonomous R7 Execution Specification

**Document ID:** PHASESHIFT-SPEC-001  
**Version:** 1.0  
**Date:** 2026-11-20  
**Status:** Implementation directive / acceptance contract  
**Target repository:** `TARGET_REPO_URL` **(the sole user-supplied input)**  
**Execution agent:** `@R7 Commit`  
**Primary language:** C++20  
**Execution environment:** CPU-only; Linux first, portable fallbacks where practical  
**Primary objective:** Build, execute, verify, benchmark, and commit a usable **algorithmic behavioral phase-transition discovery system**; no mock-only implementation, empty stubs, or unverified release claims.

---

## 0. ONE-SHOT R7 ACTIVATION DIRECTIVE

> **To R7 Commit:** Implement PHASESHIFT in `TARGET_REPO_URL`. Inspect the repository first. If empty, bootstrap it; if populated, preserve and integrate with existing working code. Treat this document as the authoritative functional and verification contract. Work autonomously through architecture, implementation, adversarial tests, benchmarks, repair, documentation, and verified atomic commits. Execute all runnable checks rather than claiming they were executed. Push verified milestones to an appropriate branch when repository permissions permit; do not force-push, merge, delete branches, modify unrelated assets, or claim a remote push without checking its result. Use meaningful milestone commits. Prioritize the core, fully executable C++20 system over UI, web services, authentication, cloud infrastructure, or elaborate deployment. Do not block implementation on open-ended architectural questions: choose the simplest documented correct option. If a requirement cannot be fulfilled in the current environment, report the exact blocker, preserve working progress, and distinguish implemented from verified. Do not leave placeholder algorithms and claim completion. Continue to the next independently verifiable milestone without requiring repetitive user approvals.

**Only requested user input:** Replace `TARGET_REPO_URL` with the intended GitHub URL. No GPU, paid model, remote API, model weights, or cloud service is needed.

### Definition of done in one sentence

A user can clone the repository, build it on a supported CPU-only Linux machine, execute self-contained algorithm comparisons, automatically discover and independently confirm at least one engineered structural transition on a deterministic fixture, replay/minimize it, inspect its provenance in a machine-readable artifact, and run the complete correctness/CI suite.

---

## 1. PRODUCT CONCEPT AND SCIENTIFIC CONTRACT

**PHASESHIFT** is an experimental algorithm analysis engine that searches a multidimensional input space for regions where algorithm performance, resource usage, or correctness changes substantially. Its distinguishing deliverable is a *reproducible structural boundary*, not merely a large runtime or a pretty plot.

### 1.1 The system must answer

1. Under **which measurable structural properties** of inputs does implementation A behave differently from B?
2. Is the difference a **continuous crossover**, a **sharp performance cliff**, an **empirical scaling change**, a **resource exhaustion boundary**, or a **correctness failure**?
3. Is that finding robust to run order, noise, random seeds, repeated measurements, and held-out inputs?
4. Can it produce the smallest or simplest **executable witness** retaining the causal property?
5. Can a separate machine reproduce the experiment, even if absolute timings and thresholds differ?
6. What evidence supports, weakens, or falsifies the claimed boundary?

### 1.2 Scientific honesty

- A measured transition is an **empirical finding**, not a mathematical theorem.
- A fitted curve is **not** proof of asymptotic complexity.
- A faster algorithm on one CPU is **not** universally faster.
- A discovered input structure is **not** automatically causally established; interventions and matched controls are needed.
- A significant difference between A and B is not by itself a *phase boundary*.
- A failure to find a boundary must be recorded as a valid negative result.
- Never claim the underlying concept is novel without a current, mechanism-level prior-art comparison.
- Never claim production-ready, research-grade, or SOTA on the basis of mere code generation or self-reporting.

### 1.3 Explicit non-goals for v1

Do **not** build a general-purpose compiler, AI/LLM agent, symbolic theorem prover, cloud dashboard, full fuzzer replacement, distributed HPC platform, untrusted-code secure sandbox, or automatically deploy generated dispatchers to users' production applications. These would distract from the measurable core.

---

## 2. IMPLEMENTATION REQUIREMENTS AND CONSTRAINTS

### 2.1 Hard constraints

- **No GPU:** no CUDA, ROCm, Vulkan compute, OpenCL, GPU kernels, or GPU dependencies.
- **C++20 production core** with modular interfaces. `std::chrono`, `std::span`, `std::jthread` where useful, RAII, explicit integer types, no undefined behavior.
- **Linux x86-64 primary target**, also compile on Linux AArch64 where feasible. Optional macOS/Windows portability must not compromise Linux delivery.
- **Default single-machine operation:** single-core measurement lane, optional parallel candidate generation/analysis.
- **Offline by default:** after installation, all standard exploration and reporting works with no network or external API.
- **Deterministic generators and replay:** identical seed, generator version, and parameters reproduce logically identical input bytes. Performance numbers can vary by machine.
- **Command-line-first**. Reports are static HTML/JSON/CSV/Markdown where practical, with no external service.
- **Bounded experiments:** explicit wall-clock, sample-count, sample-runtime, memory, input-byte, and search-node budgets.
- **Correctness gates precede ranking:** invalid outputs must never win a speed comparison.
- **No fabricated benchmarks or test outcomes.** Evidence files must result from executed programs.

### 2.2 Toolchain defaults

- CMake >= 3.24, CTest, Ninja optional.
- GCC >= 12 or Clang >= 16; record exact compiler and stdlib versions.
- Core tests with CTest; small assertion-based internal harness acceptable, or one lightweight pinned testing dependency.
- SQLite3 for experiment storage (system package or pinned vendored amalgamation, licensing and hash documented).
- JSON support via a pinned, license-reviewed C++ implementation (e.g. nlohmann/json) or a small fit-for-purpose validated parser; do not invent a fragile ad-hoc JSON parser for external inputs.
- Optional Linux perf events via `perf_event_open`/`perf`, but full functionality must work without access to counters.
- Optional Python (plot rendering and postprocessing only), not for critical search or experimental decisions.
- Optional sanitizers in Debug; no runtime dependency on sanitizer tools.

### 2.3 Build profiles

- `Debug`: warnings, assertions, AddressSanitizer/UBSan when available, tests.
- `Release`: `-O3 -DNDEBUG` as baseline; record architecture flags rather than silently assuming `-march=native`.
- `RelWithDebInfo`: profiling and performance debugging.
- CI must distinguish **portable Release** from **host-optimized Release** results.
- Treat warnings as errors on owned sources in CI when practical; do not silence real warnings.

---

## 3. MAIN USE CASES

**UC-01: Self-contained demonstration.** Run a built-in deterministic example that exposes a known artificial operation-count transition, then produce a valid boundary artifact.

**UC-02: Algorithm A versus B.** Explore sorting, shortest paths, or Top-K implementations across input morphology, validate exact semantic agreement, find performance regimes, and emit evidence.

**UC-03: Adversarial input discovery.** Discover a structure that causes a known vulnerable baseline (such as first-element-pivot quicksort on sorted data) to perform badly, with execution caps preventing quadratic blow-ups.

**UC-04: Boundary confirmation.** Confirm an exploratory candidate on independent seeds, repetitions, and nearby input settings; abstain if data are insufficient.

**UC-05: Counterexample minimization.** Reduce a failing input or performance witness while retaining the required predicate.

**UC-06: Reproducible handoff.** Export self-contained config, generator seed and parameters, environment fingerprint, raw measurement table, analysis version, witness input, and exact replay commands.

**UC-07: External algorithm integration.** Register a native C++ plugin exposing the versioned adapter interface; execute outside the main orchestrator process with error capture and resource limits.

**UC-08: Negative findings.** Report `NO_CONFIRMED_BOUNDARY` when a homogeneous/noisy workload fails to support a transition, without inventing one.

---

## 4. ARCHITECTURE

### 4.1 Execution pipeline

```text
CLI / configuration / run manifest
           |
           v
Input Morphology Engine -- deterministic generators and constraints
           |
           v
Search Scheduler -------- random / grid / adaptive / ablation baselines
           |
           v
Execution Controller ---- persistent isolated worker process(es)
           |                  |
           |                  +-- A/B algorithms + correctness oracle
           |                  +-- timing + operation/resource counters
           |                  +-- timeout / crash / memory controls
           v
Measurement Store ------- SQLite + raw evidence artifacts
           |
           v
Analysis Engine --------- normalization, uncertainty, model comparison
           |
           v
Boundary Detector ------- crossover / cliff / scaling / correctness
           |
           v
Independent Confirmer --- held-out seeds and repeat measurements
           |
           v
Witness Minimizer ------- structural simplification + revalidation
           |
           v
Discovery Ledger -------- reproducible record + reports + replay
```

### 4.2 Mandatory modules

1. `core`: types, result/status objects, seeds, hashing, version metadata, resource budgets.
2. `morphology`: family-specific generators, typed parameters, validation, encoders, structural descriptors.
3. `algorithms`: reference and candidate algorithm implementations, problem-specific correctness oracles.
4. `runner`: worker lifecycle, execution isolation, measuring, warmup, calibration, errors and timeouts.
5. `search`: deterministic grid, random baseline, adaptive proposal engine, budget accounting.
6. `analysis`: robust statistics, paired comparisons, change-point models, scaling diagnostics.
7. `confirm`: independent holdout experiments and multiple-comparison policy.
8. `minimize`: schema-aware witness shrinking, predicate preservation, failure diagnosis.
9. `storage`: schema and migrations, manifest, raw samples, query and export.
10. `report`: CLI summary, Markdown/HTML, JSON and CSV, traceable links to data.
11. `plugins`: versioned native adapter contracts, built-in registry, external worker integration.
12. `tests`: deterministic functional, negative, integration, statistical calibration, fault injection.

The modules must expose typed C++ interfaces, with low coupling and explicit ownership. Avoid single-file monoliths.

### 4.3 Repository layout (adjust only when justified)

```text
/
  CMakeLists.txt
  CMakePresets.json
  README.md
  LICENSE                    # choose only if authorized/existing; do not presume ownership
  AGENTS.md                  # concise R7 continuation and quality rules
  docs/
    architecture.md
    scientific_contract.md
    algorithm_semantics.md
    statistical_methods.md
    benchmark_protocol.md
    plugin_api.md
    reproducibility.md
    prior_art.md
    release_checklist.md
    evidence_ledger.md
  include/phaseshift/
    core/ morphology/ runner/ search/ analysis/ confirm/
    minimize/ storage/ report/ plugins/
  src/
    core/ morphology/ algorithms/ runner/ search/ analysis/
    confirm/ minimize/ storage/ report/ plugins/ cli/
  apps/
    phaseshift.cpp
    phaseshift_worker.cpp
  plugins/examples/
  configs/
    demo_transition.json
    sorting_quicksort.json
    graph_density.json
    topk_fraction.json
    negative_control.json
  tests/
    unit/ integration/ fixtures/ statistical/ adversarial/
  benchmarks/
    baselines/
    protocols/
  scripts/
    ci_smoke.sh
    verify_repro.sh
  .github/workflows/
    build-test.yml
    sanitizers.yml
  artifacts/README.md          # only examples/manifests in git; no massive generated blobs
```

Do not commit gigabytes of benchmark data. Keep small golden fixtures in source control; write normal run data to ignored `runs/`.

---

## 5. CANONICAL DATA AND ALGORITHM CONTRACTS

### 5.1 Data model

A case is an immutable tuple:

```text
CaseId = H(schema_version || family_id || generator_version ||
           canonical_parameter_bytes || seed || canonical_input_bytes)
```

Persist actual input bytes or an input artifact checksum and a verified regeneration path. Use a stable, documented hash (e.g. SHA-256 via pinned implementation or stable single-function implementation); do not use implementation-defined `std::hash` for persistent identities.

Conceptual types:

```cpp
struct CaseDescriptor {
    std::string family_id;
    std::string generator_version;
    std::uint64_t seed;
    ParamMap parameters;                 // typed, canonical, validated
    std::vector<std::byte> input_bytes;  // exact canonical test case
    MorphologyFeatures features;         // derived and labeled
};

enum class RunStatus {
    ok, wrong_answer, invalid_input, timeout, out_of_memory,
    signaled, crashed, counter_unavailable, infrastructure_error
};

struct Sample {
    std::string case_id;
    std::string algorithm_id;
    RunStatus status;
    std::uint64_t wall_ns;
    std::optional<std::uint64_t> cpu_ns;
    std::optional<std::uint64_t> cycles;
    std::optional<std::uint64_t> instructions;
    std::optional<std::uint64_t> work_units;
    std::uint64_t peak_rss_bytes;
    std::uint32_t batch_iterations;
    std::uint32_t paired_block;
    std::uint64_t order_seed;
    std::string output_digest;
};
```

Exact names may change, but all semantics must survive.

### 5.2 Algorithm comparison rules

- A and B must receive **logically identical** immutable inputs; if the API mutates input, clone it outside the timed region and document copy cost separately.
- Time only the declared operation (e.g. sorting, not input generation); separately record end-to-end time if useful.
- Define the output equivalence relation per problem and compare every candidate run with a trusted oracle under the same contract.
- For parallel algorithms, specify thread counts; default comparisons are single-threaded.
- All algorithms must be identified by name, source revision, compile flags, and adapter version.
- Inadmissible result states (`wrong_answer`, `timeout`, crash) are not treated as valid finite timings or hidden from reports.
- Allocation cost is **included by default** when the algorithm itself allocates; input construction and output validation are outside the operation timing.
- Ensure work is not optimized away: digest results via an observable sink outside timed code, with compiler optimization sanity checks.
- Each case must define valid input range; invalid/unsupported combinations are rejected, not silently coerced.

### 5.3 Plugin ABI v1

Create a small **stable C ABI** for native plugin registration, avoiding C++ ABI instability across compiler builds. A C++ wrapper may be provided. Define functions equivalent to:

```c
/* Header must be self-contained, versioned, and compiled in an example plugin. */
#define PHASESHIFT_PLUGIN_ABI_VERSION 1

typedef struct {
    const unsigned char *data;
    size_t len;
} ps_input_v1;

typedef struct {
    unsigned char *data;
    size_t len;
    void (*release)(unsigned char *data, size_t len);
} ps_output_v1;

typedef struct {
    uint32_t abi_version;
    const char *algorithm_id;
    int (*validate_input)(ps_input_v1 in);
    int (*run)(ps_input_v1 in, ps_output_v1 *out);
    void (*reset)(void);                /* optional; document state model */
} ps_algorithm_v1;

/* Host looks this up via dlsym(). */
const ps_algorithm_v1 *phaseshift_get_algorithm_v1(void);
```

These types illustrate the contract; actual header must explicitly define error codes, ownership, `size_t` inclusion, output-allocation protocol, maximum output sizes, null-buffer handling, and exception boundaries. Never allow C++ exceptions to cross the C ABI. Reject version mismatch with a clear error. Build/test one real example `.so`.

**Isolation design:** The orchestrator launches a persistent **child worker** and loads each shared library in the child, not into the orchestrator. Use length-prefixed, versioned binary IPC, bounded payloads, and safe process termination. Worker may be persistent for valid latency measurement; provide spawn-per-sample isolation mode when persistent state may contaminate results. Running native user plugins in a child is fault containment, **not a complete security sandbox**. Require trusted plugins for v1. Do not claim seccomp/container-level security unless independently implemented and verified.

---

## 6. INPUT MORPHOLOGY ENGINE

### 6.1 Purpose

Generate structured experimental families with interpretable dimensions, not untyped random blobs.

Every generator must implement:

```text
schema() -> parameter types, ranges, dependencies
validate(params) -> typed success/error
synthesize(params, seed) -> bytes
features(bytes) -> measured morphology vector
shrink(bytes, constraints) -> candidate simpler valid cases
```

The same `generator_version`, seed, and parameters must yield identical byte streams on supported platforms. Implement PRNG explicitly (e.g. SplitMix64 / PCG with precisely specified bit operations) and test golden outputs. Avoid compiler-specific distributions for cross-platform reproducibility.

### 6.2 Mandatory families

#### F1: Integer-array family

Parameters:
- `n` (integer, supported bounded range)
- `distribution`: uniform, sorted ascending, sorted descending, nearly sorted, organ-pipe, sawtooth, alternating, all equal, heavy duplicate, few unique, Zipf-like/skewed with fixed deterministic algorithm
- `duplicate_fraction` / `unique_count`
- `disorder_fraction` / swap count
- `run_length`, `value_range`, `sign_fraction`, `seed`

Features:
- observed `n`, number of unique values, inversion estimate or exact count for small n, adjacent disorder, max repeated run, entropy estimate, range width, duplicate ratio.

Mandatory algorithm implementations:
- `std::sort` reference (int64 values with ordinary ascending total order)
- deterministic first-pivot quicksort **explicitly labeled vulnerable teaching baseline**, with bounded recursion/stack checks or iterative implementation
- an independent, correct mergesort
- insertion sort for small inputs (not a universal high-n competitor)

Define whether the comparison is of sorted value sequences or stable relative ordering; for ordinary integer arrays the output contract is exact ascending values.

#### F2: Top-K family

Parameters:
- `n`, `k` with `0 <= k <= n`
- same array distributions
- `k_fraction`, duplicate ratio, skew

Algorithm implementations:
- reference full sort descending
- bounded min-heap selection
- `std::nth_element` + final sort, with `k=0` and `k=n` explicitly handled

Contract: return the first `k` values in **descending numeric order**, duplicates retained. Equal values are identical under value-only semantics. If element identity is later added, define deterministic `(value, original_index)` ordering and update all paths consistently.

#### F3: Nonnegative shortest-path graph family

Parameters:
- `vertices`, `edges` or `density`, directed flag, weak/strong connectivity controls if relevant, degree skew, community count, source, weight distribution, `seed`
- `weight_class`: zero-one or bounded nonnegative integral
- graph construction explicitly prevents invalid indices and out-of-budget allocation

Algorithms:
- trusted Bellman-Ford for small graphs as an independent oracle (or independently validated exhaustive checker for tiny graphs)
- binary-heap Dijkstra
- dense/linear-scan Dijkstra
- 0–1 BFS **only** on the restricted `{0,1}` edge-weight domain

Contract: source-to-all-vertices distances in fixed-width integer representation, with documented `INF` for unreachable and overflow-safe addition. For general nonnegative weights, do not select 0–1 BFS. Sorting adjacency and tie semantics must not change distance correctness. Duplicated edges and self-loops require declared policy and tests.

#### F4: Deterministic operation-count transition (calibration fixture)

Provide one pure, fully deterministic micro-workload with an explicit threshold `t` in an input structural parameter `x` such that declared `work_units` changes across `t` by a known amount **without using wall-clock delay or sleeps**. This verifies detector logic independent of CPU noise. The engine must recover the boundary within the configured discrete resolution. The fixture is synthetic and **must never be marketed as a discovered algorithmic phenomenon**.

### 6.3 Constraints and generation safety

- Invalid combinations are rejected with precise diagnostics.
- Enforce estimated input-memory budgets before allocation; cap vertex-edge products with checked arithmetic.
- Generator must record requested and **observed** features; never assume intended morphology was achieved.
- Do not conflate input size `n` with a structural dimension such as density.
- Sample genuinely independent seeds for holdout; never reuse discovery cases during confirmation.
- For structural intervention, construct matched pairs preserving every feasible nuisance property other than the manipulated factor.

---

## 7. MEASUREMENT ENGINE

### 7.1 Separate correctness and timing

For each candidate case:

1. Validate generator schema and case checksum.
2. Run a correctness oracle or cross-check; capture output digest and full mismatch diagnostics (capped in size).
3. If valid, warm up the target execution path and calibrate a batch size so measured time exceeds timer granularity.
4. Execute paired, randomized or balanced A/B measurement blocks.
5. Store **raw samples**, outcomes, and environment metadata before analysis.
6. Record failures and censored observations distinctly; never replace them with magic-large runtime values.

### 7.2 Experimental noise controls

- Record CPU model, core count, OS/kernel, compiler/flags, locale, core affinity (if set), governor information (if available), process priority, temperature/frequency telemetry when accessible, available perf counters, run time and machine ID surrogate (non-identifying).
- Default one measurement lane per machine; no simultaneous competitor timings.
- Balanced execution order `ABBA` or seed-randomized balanced blocks; report actual order.
- Compare medians / robust summaries with paired analysis where possible; present full distributions and CI.
- Perform a minimum warmup; calibration iterations are **not** treated as independent observations.
- For tiny operations, measure batched repeated invocations with well-defined state reset and digest, and report both batch and per-invocation estimates.
- Detect outlier instability and high noise; do not silently trim samples to make a result significant. Predeclare robust treatment.
- Optional CPU pinning and `perf_event_open`; if unavailable, emit warnings and fall back cleanly to monotonic clock timing.
- Use `steady_clock` after checking its behavior; record the clock source/proxy where practical. Report nanoseconds as measured, not as guaranteed precision.
- Re-run on fresh process when cache state, allocator state, or stateful plugins threaten validity.

### 7.3 Resource limits

Mandatory config keys with sensible defaults:

```json
{
  "limits": {
    "total_wall_seconds": 600,
    "max_cases": 2000,
    "max_case_bytes": 67108864,
    "max_worker_rss_mb": 1024,
    "max_sample_wall_ms": 3000,
    "max_report_bytes": 10000000,
    "max_search_queue": 10000
  }
}
```

These are example defaults, not promises of host-enforced memory isolation. On Linux implement applicable `setrlimit` checks, parent-side wall timeout, process-group kill/reap, bounded pipe reads and size validation; document remaining limitations. Allow stricter user budgets. Never execute unescaped shell commands or interpolate arbitrary user input into a shell.

### 7.4 Counters and interpretations

- Primary metric: **time per completed operation**, with failure states separated.
- `work_units` is an *instrumented algorithm-specific unit*, not automatically comparable across different algorithms unless definitions match.
- Optional metrics: CPU time, retired instructions, cycles, cache misses, allocations/bytes, peak resident memory.
- Counter collection itself may alter runtime; where necessary use separate passes for counters and latency.
- Store raw numerator and denominator alongside normalized values. A missing hardware counter is `null`, never zero.

---

## 8. SEARCH AND EXPLORATION

### 8.1 Common budgeted search API

Every strategy obeys the same deterministic seed, budget, evaluation callback, semantic constraints, and data recording. Implement:

1. **Grid sweep** on small discrete dimensions (reliable baselines and ground-truth fixtures).
2. **Uniform / stratified random search** as required comparator.
3. **Adaptive search** that focuses on uncertain or potentially high-contrast regions while preserving a fixed exploration fraction.

### 8.2 Recommended adaptive algorithm v1

Begin with a coarse **space-filling design** or bounded grid over 1–3 selected interpretable morphology dimensions. For each evaluated cell, calculate performance ratio uncertainty and nearby structural gradients. Maintain candidate edges between adjacent evaluated bins/cells. Allocate budget according to:

```text
priority(edge) = normalized_boundary_evidence
               × expected_uncertainty_reduction
               × structural_feasibility
               ÷ estimated_measurement_cost
```

Keep a minimum (e.g. 20%) of budget for uniform exploration so the scheduler does not collapse onto the first noisy hotspot. Subdivide cells adjacent to plausible boundaries. Cache case identities; deduplicate exact repeats except intentional replications. Persist scheduler state so an interrupted search can resume deterministically.

For categorical features, search edges between distinct categories or condition on category; never pretend categories form a continuous real-valued axis. For correlations, mark confounded dimensions and seek matched interventions.

**Failure prevention:** Do not implement a giant Bayesian optimizer as a prerequisite. A simple adaptive partition / priority-queue search with calibrated uncertainty is acceptable and testable.

### 8.3 Ablation baselines

Compare adaptive against grid, uniform random, and (where applicable) stratified random under **equal numbers of case evaluations and comparable runtime budgets**. The ablation must evaluate hit rate, confirmed boundaries, search overhead, and total CPU seconds. If adaptive search fails to beat a baseline, publish the negative result.

---

## 9. STATISTICS, DETECTION, AND CLASSIFICATION

### 9.1 Primary comparison variable

For a valid A/B pair on case `c`:

```text
r(c) = log( median_runtime_A(c) / median_runtime_B(c) )
```

A negative `r` means A is faster, positive means B is faster. `r=0` indicates equal medians. Also report ratio as `exp(r)` and explicit sample distributions. Do not divide by zero: enforce measurable positive durations and mark below-resolution cases. For stable deterministic operation-count fixtures, use the separate work-count metric and state that this is **not wall-clock performance**.

### 9.2 Evidence taxonomy (required)

- `PERFORMANCE_CROSSOVER`: the sign of the A-vs-B performance advantage changes across nearby structural values, with confirmed estimates on both sides.
- `PERFORMANCE_CLIFF`: large local rise/fall in the chosen algorithm's normalized performance measure over a small structural change, exceeding calibrated noise and a predeclared practical-effect threshold.
- `SCALING_REGIME_CHANGE`: evidence that the empirical slope or shape across multiple `n` ranges changes; label **empirical**.
- `MEMORY_OR_RESOURCE_BOUNDARY`: resource pressure or recorded timeout/OOM threshold; report censoring and machine limits.
- `CORRECTNESS_BOUNDARY`: deterministic region where a candidate violates the oracle or crashes; correctness finding, not a speed win.
- `UNSTABLE_OR_INCONCLUSIVE`: promising but insufficient confirmation, nonstationary data, or contradictory holdout.
- `NO_CONFIRMED_BOUNDARY`: robust negative outcome.

Never use an unspecified generic word `phase transition` in a report without its empirical subtype and operational definition.

### 9.3 Stage A: detection (exploratory)

- For 1D ordered scans, implement at least one explicit segmented model or change-point technique and compare it against a no-boundary null model. Penalize complexity; do not favor more segments automatically.
- For 2D continuous axes, start with adjacent-cell contrasts and adaptive refinement; store the observed grid, not a fictional smooth continuous surface.
- Define practical thresholds in configuration, e.g. candidate effect ratio >= `1.20` (tunable); justify rather than hardwire a universal threshold.
- For continuous crossover, use sign change plus a band of uncertainty, **not** a discontinuity requirement.
- For cliffs, compare local differences against effect and uncertainty thresholds, with normalization for changing `n`.
- Do not confuse gradual smooth trends with an abrupt changepoint; label as crossover or smooth gradient if justified.
- Track each hypothesis, its exploration data, rank, and reason for selection.

### 9.4 Stage B: independent confirmation (mandatory before `CONFIRMED`)

- Freeze the hypothesis, variable, effect direction, structural window, decision rules, and chosen algorithms before holdout sampling.
- Generate independent seeds / cases not used in discovery and measure with new randomized paired blocks.
- Use confidence intervals based on an appropriate **paired** bootstrap / randomization procedure; report the resampling unit (case, paired block) and do not pseudo-replicate iterations within one timed batch.
- Confirm practical magnitude and appropriate statistical evidence (default two-sided 99% CI for a predeclared primary contrast, with configurable conservatism).
- Where testing many candidate boundaries, apply an explicit multiplicity policy (e.g. Benjamini–Hochberg FDR across the final family) or use a rigorously isolated confirmation set and document its limits.
- Test neighboring points on **both sides** of proposed boundary, plus one distant negative-control point.
- Report estimated boundary location and an uncertainty interval or discrete resolution. If it cannot be localized, say so.
- If confirmation fails, change status to `NOT_CONFIRMED` and keep all original exploration data. Never discard inconvenient runs.

**Default sample plan** (configuration can override based on precision and costs): at least 9 paired blocks per discovery case, and at least 20 paired confirmation blocks per key neighborhood case. Counts are initial rules, not an assurance of statistical power; the engine must be capable of collecting more within budget or abstaining.

### 9.5 Scaling fingerprinting

- Measure `n` on a declared schedule, including n ranges not used to discover the hypothesis.
- Use data-dependent fits to candidate families (`constant`, `log n`, `n`, `n log n`, `n^2`, `n^3` where meaningful). Fit constants on positive runtime or work counts with a documented robust method; compare residuals/held-out error.
- Require at least 6 useful `n` values for a basic scaling fit, preferably more, and multiple independent seeds at each size.
- Report sensitivity to range selection and confidence/fit diagnostics.
- Do not report `O(n^2)` as proven; use wording such as "observed growth compatible with quadratic scaling over n ∈ [a,b]".
- Account for other varying factors (density, `k`, graph weights) in the declared problem model; avoid conflating `E` and `V` in graph complexity.

---

## 10. CAUSAL/STRUCTURAL WITNESS MINIMIZER

### 10.1 Witness contract

A witness consists of:
- exact replayable input and digest;
- candidate/algorithm versions;
- discovery hypothesis and predeclared **preservation predicate**;
- measured evidence and oracle status;
- shrink trace;
- final minimized case or simpler parameter set;
- independent re-confirmation after minimization.

Support two predicates:

1. **Correctness witness:** candidate miscomputes/crashes while oracle succeeds, reproducibly.
2. **Performance witness:** effect magnitude exceeds chosen practical threshold with credible repeated evidence, under matched constraints.

Do not shrink based on a single noisy timing.

### 10.2 Implementation

- Start with hierarchical parameter simplification: reduce `n`, edge count, distinct values, magnitude, sortedness perturbations, etc., using explicit validity constraints.
- Then use family-aware `ddmin`-style structural reductions: array subranges, graph vertices/edges with renumbering and source reachability contract, values/weights, and permutation edits.
- Cache failed/passed candidate fingerprints and reuse measurements only when versions and environment permit.
- Preserve semantics; reject invalid shrinks.
- Prioritize lexicographically: smaller serialized bytes, then structural complexity, then simpler descriptors (document exact order).
- After shrink, rerun the independent validator with a fresh seed/order plan and record whether the predicate remains confirmed.
- `MINIMIZED_CONFIRMED`, `MINIMIZED_PROVISIONAL`, and `MINIMIZATION_FAILED` are distinct statuses.

Generate a standalone **C++ reproduction file** for built-in algorithms, or a small input+runner package for plugins when standalone source cannot be embedded. All generated source must be deterministic and compile under the documented toolchain.

---

## 11. REPRODUCIBILITY, PERSISTENCE, AND EVIDENCE

### 11.1 Mandatory stored entities

Use a versioned SQLite schema with migrations and foreign keys:

- `experiments`: purpose, immutable config hash, creation time, status, schema and engine versions.
- `environments`: compiler/kernel/CPU/flags/affinity/counters fingerprint, measurement policy.
- `algorithms`: name, version/source commit, ABI, output contract.
- `generators`: family, schema and generator version.
- `cases`: case ID, family, typed parameters, observed features, seed, digest, stored input location.
- `samples`: exact raw measurement or failure with pairing/order metadata.
- `aggregates`: computed medians, uncertainty, derived features, calculation version.
- `hypotheses`: proposed boundary, source cases, candidate state, score, frozen definition.
- `confirmations`: holdout samples and verdict.
- `witnesses`: original, shrink path, minimized case, reproduction artifact.
- `artifacts`: content digest, role, file path, version.
- `events`: append-only lifecycle decisions, rationale and evidence links.

Use schema migrations rather than destructive recreation. Enable WAL where suitable; use transactions. Do not store security-sensitive host paths, tokens, or usernames in public example reports.

### 11.2 Discovery evidence state machine

```text
OBSERVED -> HYPOTHESIZED -> TESTED -> CONFIRMED -> MINIMIZED_CONFIRMED
                                     |          |
                                     |          +--> MINIMIZATION_FAILED
                                     +--> NOT_CONFIRMED

Any stage --> INVALIDATED (if source correctness, config, code, or evidence invalid)
```

A `CONFIRMED` discovery must identify independent holdout evidence. No status is silently promoted by regeneration of reports.

### 11.3 Exact replay contract

- `phaseshift replay --run <id> --case <case-id> --db runs/...sqlite`
- Reconstruct exact input and validate digest; compare known oracle output; optionally repeat performance measurements on current hardware.
- If replay hardware differs, preserve prior raw data and add a new environment-specific measurement; never overwrite original observations.
- `phaseshift export` writes a portable evidence bundle including readme, config, schema versions, algorithm identifiers, input bytes, raw CSV/JSON, and reproduction commands.
- Import verification checks checksums and rejects incompatible versions with clear diagnostics.

### 11.4 Reporting

Every report must clearly display:
- objective and compared implementations;
- CPU/software environment;
- family and structural axes;
- budget/exact evaluated case counts;
- correctness failures;
- raw/aggregate effect sizes and uncertainty;
- discovered and held-out sample counts;
- `CONFIRMED` vs `PROVISIONAL` vs negative findings;
- boundary coordinates/resolution and witness path;
- exact CLI replay command;
- methodological limitations and alternatives.

Static HTML visualization optional but useful: use an embedded data file or inline small data; no CDN or JavaScript dependency required for basic viewing. Do not smooth a sparse heatmap without marking interpolation. CSV and JSON are nonoptional.

---

## 12. COMMAND-LINE UX AND CONFIGURATION

### 12.1 Required CLI commands

```bash
phaseshift --version
phaseshift doctor
phaseshift list families
phaseshift list algorithms
phaseshift validate --config configs/sorting_quicksort.json
phaseshift demo --out runs/demo
phaseshift explore --config configs/sorting_quicksort.json --out runs/sort1
phaseshift confirm --run runs/sort1 --hypothesis <id>
phaseshift minimize --run runs/sort1 --hypothesis <id>
phaseshift replay --run runs/sort1 --case <case-id>
phaseshift report --run runs/sort1 --format markdown,json,csv,html
phaseshift export --run runs/sort1 --out runs/sort1_bundle
phaseshift compare --a runs/experiment1 --b runs/experiment2
```

Exact option naming may be normalized, but **all advertised commands must work**; no placeholder commands. Provide `--help`, structured exit codes and human-readable errors. `doctor` must not require privileged Linux performance counters. `compare` must reject mismatched semantic contracts and prominently flag differing hardware environments.

### 12.2 Config v1 example

```json
{
  "schema_version": 1,
  "experiment_id": "sort-structure-001",
  "problem_family": "integer_array",
  "algorithms": ["std_sort", "quicksort_first_pivot"],
  "objective": "discover_structural_performance_boundaries",
  "seed": 137,
  "axes": {
    "n": {"values": [64, 128, 256, 512, 1024, 2048, 4096]},
    "distribution": {"values": ["uniform", "ascending", "descending", "all_equal", "nearly_sorted"]},
    "duplicate_fraction": {"values": [0.0, 0.25, 0.5, 0.9]}
  },
  "fixed": {"value_range": 1000000},
  "search": {
    "method": "adaptive_partition",
    "exploration_fraction": 0.20,
    "max_cases": 1500
  },
  "measurement": {
    "paired_discovery_blocks": 9,
    "paired_confirm_blocks": 20,
    "warmup_iterations": 5,
    "minimum_batch_ms": 2,
    "primary_metric": "wall_ns_per_operation"
  },
  "detection": {
    "min_practical_ratio": 1.20,
    "confirmation_confidence": 0.99,
    "multiple_testing": "bh_fdr",
    "fdr_q": 0.05
  },
  "limits": {
    "total_wall_seconds": 600,
    "max_cases": 1500,
    "max_case_bytes": 67108864,
    "max_worker_rss_mb": 1024,
    "max_sample_wall_ms": 3000,
    "max_report_bytes": 10000000,
    "max_search_queue": 10000
  }
}
```

**Validation notes:** For categorical `distribution`, not every `duplicate_fraction` is feasible/meaningful. Configuration validation must either reject incoherent combinations or explicitly normalize to an observed feature and document how. For an A/B run, never silently replace a named algorithm with a different one.

---

## 13. REQUIRED ALGORITHMIC EXPERIMENTS AND BENCHMARKS

### 13.1 Core benchmark suites

| ID | Case | Intended verification | Required verdict behavior |
|---|---|---|---|
| B01 | Synthetic operation-count threshold | Known transition coordinate / deterministic count difference | Must detect known boundary at declared grid resolution |
| B02 | Synthetic constant/no-change count | Null calibration | Must abstain; no invented boundary |
| B03 | First-pivot quicksort versus `std::sort` | Structured pathology on sorted/duplicate classes | Must verify correctness; produce raw comparison and seek a supported finding |
| B04 | Integer array sorting, random families | General-purpose morphology/ratio mapping | Valid input/output and artifact generation; finding not guaranteed |
| B05 | Heap Top-K versus `nth_element` | `k/n` dimension with duplicates | Correct output and maps; finding not guaranteed |
| B06 | Heap vs dense Dijkstra | Graph density / vertex-edge influence | Correct distances; finding not guaranteed |
| B07 | 0–1 BFS vs Dijkstra on {0,1} weights | Restricted semantic compatibility | Strict domain checks, raw measurements |
| B08 | Incorrect intentionally injected algorithm | Correctness gate | Must mark `wrong_answer`, exclude from speed winner |
| B09 | Crash, OOM/timeout controlled plugin | Process fault handling | Worker failure isolated, logged, bounded |
| B10 | Two size regimes with deceptive calibration | Generalization test | Reject claims unsupported by held-out n |
| B11 | Randomized run order / unstable timing | Noise treatment | Signal flagged unstable/inconclusive where needed |
| B12 | Restart interrupted search | Determinism/resume | Same case identities and scheduler decisions for same persisted state |

Do not require hardware-specific crossovers in CI; tests of detection rely on deterministic synthetic signals. Real-world performance suites must publish actual outcomes, including negative findings.

### 13.2 Comparative research baselines

Implement equal-budget comparison among:
- grid search (where feasible),
- uniform random search,
- stratified random search,
- PHASESHIFT adaptive search.

Evaluate boundary localization error on deterministic synthetic controlled cases; discovery yield / useful confirmed findings on real algorithm cases; total evaluations; total CPU time; confirmation false-positive rate on null tests; and witness size reduction.

For any external tool comparison (PerfFuzz, SlowFuzz, AFL variants), verify that target inputs, objectives and measurement budgets are compatible. If not, state that direct quantitative comparison is unsupported. These tools are **prior art**, not claimed built-in dependencies.

### 13.3 Metrics and release scorecard

- Correctness oracle agreement rate (for valid implementations): 100% of completed valid cases.
- Synthetic transition localization: exact discrete threshold or within one declared grid bin, as appropriate.
- Null false discoveries: zero **confirmed** boundaries in the prescribed deterministic null suite.
- Replay input equality: exact bytes/digest in 100% of golden tests.
- Tested worker outcomes: normal, invalid, timeout, signal, malformed IPC, oversized payload, exception, and version mismatch.
- Run completion: budget-respecting with no runaway workers and no unhandled termination.
- Statistical validity: test CI/multiplicity implementation against deterministic reference computations and simulated null distributions with explicit seed.
- CI: compiler matrix + sanitizers + unit/integration, with documented platform limits.
- No numeric performance speedup target is guaranteed or required for release; acceleration is empirical.

---

## 14. VALIDATION MATRIX AND ADVERSARIAL QUALITY GATES

### G0 — Repository / environment safety

- Inspect existing code, branches, README, LICENSE, CI, uncommitted changes and remote first.
- Never overwrite unrelated work, alter licenses without authorization, or mutate remote state destructively.
- Log actual compiler/execution capabilities; select CPU-only design.

### G1 — Correct reference algorithms

- Sorting: thousands of deterministically generated arrays, sizes `0,1,2,...`, negative values, duplicates, max/min integers, already ordered/reversed, very large permitted values.
- Top-K: `k=0`, `k=1`, `k=n`, `k>n` invalid; ties, negatives, repeated values, empty arrays.
- Graphs: disconnected, singleton, zero edges, zero weights, parallel edges, self-loops, dense, sparse, path/star, integer near overflow, invalid source, 0–1 restriction.
- Differential against independent references; fixed seed reproducibility.
- All admissible outputs match defined semantics; failures block milestone.

### G2 — Harness integrity

- Input generation and cloning excluded from reported core operation time.
- Batching does not change semantics; optimizer cannot eliminate measured work.
- Measurement parser does not interpret missing counters as zero.
- Worker exceptions/crashes/timeouts do not crash orchestrator; status preserved.
- At least two build configurations tested; sanitize owned code.

### G3 — Boundary inference

- Known synthetic boundaries detected.
- Smooth monotonic functions not misreported as discrete cliffs.
- Synthetic nulls produce no confirmed boundary.
- Holdout never draws same case fingerprints as discovery.
- Multiple-candidate selection cannot reuse confirm data opportunistically.
- Model selection and uncertainty functions have unit tests with reference expected answers.

### G4 — Minimization and replay

- Shrinking preserves validity and witness predicate.
- Minimized input is no larger according to declared lexicographic ordering.
- Replay checks bytes/digests and revalidates witness.
- Wrong-answer witness reproducible independent of timing; noisy performance witness must re-confirm or be provisional.

### G5 — Reproducible comparisons

- Raw observations persisted before aggregated claims.
- Baseline versions/flags/hardware captured.
- Same dataset and equal semantic output contract.
- Equal-budget search comparisons.
- Negative/contradictory results included.

### G6 — Production-quality readiness

- `cmake --build` and `ctest` pass on supported Linux.
- CI workflows execute on GitHub Actions with least privilege; do not assume absent runner success.
- ASan/UBSan clean on available runner(s); concurrency checks if parallel code exists.
- Deterministic smoke demo completes under a declared modest CI time budget (preferably <= 5 minutes on standard hosted CPU runners).
- README installs/builds/runs from clean checkout and aligns with actual binaries.
- No placeholders, unimplemented commands falsely advertised, fabricated chart samples, or meaningless coverage inflation.

**Release rules:** A feature with an unpassed mandatory gate is labelled incomplete or experimental. Never weaken tests or thresholds merely to create a green badge.

---

## 15. ENGINEERING MILESTONES AND ATOMIC COMMITS

R7 should run the milestones autonomously and commit after each **verified** stable increment, not after arbitrary output chunks. Suggested names are illustrative:

1. **M0: Repo inventory + toolchain** — `build: establish CPU-only C++20 project and CI skeleton`.
2. **M1: Contracts + references** — `feat: add typed algorithm contracts and correctness oracles`.
3. **M2: Morphology** — `feat: add deterministic structured case generators`.
4. **M3: Runner** — `feat: implement isolated worker, bounded execution and raw measurement`.
5. **M4: Data** — `feat: persist versioned experiments and reproducibility metadata`.
6. **M5: Search** — `feat: implement budgeted grid, random and adaptive exploration`.
7. **M6: Analysis** — `feat: add candidate boundary models and robust A/B summaries`.
8. **M7: Confirmation** — `feat: add independent holdout confirmation and evidence states`.
9. **M8: Minimization** — `feat: add schema-preserving witness shrink and replay export`.
10. **M9: User product** — `feat: complete CLI, reports and native plugin sample`.
11. **M10: Research/evaluation** — `test: add calibration fixtures, adversarial suites and fair ablations`.
12. **M11: Hardening** — `chore: close CI, sanitizers, docs and release acceptance gates`.

**Each commit must:** have a coherent diff; compile; pass tests relevant to touched paths; include a short evidence note; not overwrite unrelated code; preserve previously verified invariants. If a milestone is too large, split it into smaller meaningful atomic commits. Use a feature branch if required by repository policy; prefer PR over direct main push when project conventions demand it. Never claim a push succeeded without inspecting Git's remote response and final SHA.

### R7 execution control

- **Autonomy:** Do not request repeated user decisions. Handle tradeoffs by documenting them in ADR-style notes.
- **Core-first:** Algorithm and measurement correctness first; no auth, SaaS, payments, account infrastructure, heavy GUI, or external services.
- **Repair loop:** implement → execute → inspect failure → minimize counterexample → repair → rerun regression → commit.
- **Evidence-only claims:** record test command, actual exit code, artifact path, compiler, date/time, and commit SHA.
- **No empty ceremony:** No dozens of pseudocode-only milestones, TODO-only stubs, or prose substitutes for execution.
- **Safe fallbacks:** If full feature requires infrastructure unavailable in agent runtime, implement a smaller executable path and clearly mark the missing verified gate. Do not present unexecuted code as tested.
- **Pause condition:** Only stop for genuine authorization blockers (credentials, protected branches, destructive action, inaccessible repo), unsolvable execution blockers, or completed acceptance contract. Preserve verified state on interruption.

---

## 16. ADVANCED CAPABILITIES — POST-v1, NOT BLOCKING RELEASE

These are extension targets only after G0–G6 pass:

### A. Phase-aware automatic dispatcher

Learn a simple interpretable decision rule from training experiments to select among semantically equivalent algorithms by observed morphology. Verify on a separate held-out dataset. Include cost of morphology feature extraction and dispatch overhead in end-to-end comparisons. Always retain the trusted safe fallback and prohibit unsupported inputs. Generate code only when equivalence and improvement are empirically supported; never claim global optimality.

### B. Multi-dimensional boundary topology

Build boundary graphs/maps across 2–4 structural axes with sparse sampling and explicit uncertainty, plus categorical splits. No false implication that interpolated regions have been measured.

### C. Statistical sequential design

Explore sequential tests, error-spending, active learning and uncertainty-aware search with proper multiple-comparison control. Benchmark against simple adaptive partition first.

### D. Metamorphic oracles

For problems without cheap exact references, add relation-based validation (permutation invariance for sorting, graph relabeling invariance, etc.), still clearly distinguished from complete correctness proof.

### E. Performance provenance across commits

Track regressions and new behavior boundaries between builds on comparable hardware, run history, and compiler settings. Optional git bisect integration with strict guardrails.

### F. Formal claims and stronger novelty study

For an especially promising result, derive a mathematical explanation, try to disprove it, and produce a separate paper-style artifact. Research novelty is **not** assumed by the engineering project.

---

## 17. PRIOR ART AND ENGINEERING REFERENCES

R7 must inspect closest mechanisms and record overlap/differences in `docs/prior_art.md`. These references contextualize design; they do not certify novelty.

1. **PerfFuzz** — generates pathological inputs with performance-directed fuzzing: https://github.com/carolemieux/perffuzz ; paper: https://people.eecs.berkeley.edu/~ksen/papers/perffuzz.pdf
2. **SlowFuzz** — algorithmic complexity vulnerability discovery: https://arxiv.org/abs/1708.08437
3. **LLVM benchmarking guidance** — CPU measurement, noise and reproducibility considerations: https://llvm.org/docs/Benchmarking.html
4. **Linux `perf_event_open`** — optional CPU counter interface and constraints: https://man7.org/linux/man-pages/man2/perf_event_open.2.html
5. **Change-point detection / PELT reference implementation** — methodological inspiration, not proof of fit for input morphology: https://github.com/deepcharles/ruptures/blob/master/docs/user-guide/detection/pelt.md

**Preflight requirement:** Before claiming unique research contribution, search at least performance-directed fuzzing, worst-case input synthesis, algorithm configuration/selection, active benchmarking, change-point detection, delta debugging, and differential performance testing. Build a concise mechanism-comparison table and identify what experimental result would falsify PHASESHIFT's differentiation claim.

---

## 18. REQUIRED FIRST DEMONSTRATION (END-TO-END ACCEPTANCE)

R7 must produce and actually run a command sequence equivalent to:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure

./build/phaseshift doctor
./build/phaseshift demo --out runs/acceptance_demo
./build/phaseshift explore \
  --config configs/sorting_quicksort.json \
  --out runs/sorting_experiment
./build/phaseshift report \
  --run runs/acceptance_demo \
  --format markdown,json,csv,html
```

Then demonstrate at least one **confirmed synthetic work-unit boundary** (not a fabricated performance finding), expose its original and minimized witness, execute replay, and show separate real timing experiments whose results are honestly reported as confirmed/provisional/negative.

The final R7 handoff must contain:

```text
REPO URL:
BRANCH:
FINAL COMMIT SHA:
COMMITS CREATED:
SUPPORTED OS / COMPILERS:
BUILD COMMAND + RESULT:
TEST COMMAND + RESULT:
SANITIZER COMMAND + RESULT:
DEMO COMMAND + RESULT:
REFERENCE ALGORITHM AGREEMENT:
SYNTHETIC BOUNDARY FOUND + GROUND TRUTH:
REAL ALGORITHM FINDINGS + EVIDENCE STATUS:
REPLAY / MINIMIZATION RESULT:
SEARCH ABLATION RESULTS:
ARTIFACT PATHS / DOWNLOADABLE REPORTS:
GATES PASSED / NOT PASSED:
KNOWN LIMITATIONS:
NEXT SMALLEST VERIFIED STEP:
```

Do not replace these with qualitative assertions. A gate is `PASS`, `FAIL`, `NOT_RUN`, or `BLOCKED`, with pointers to actual evidence.

---

## 19. COMPLETION DEFINITION AND PRIORITIZATION

### Mandatory for v1.0

- CPU-only C++20 build; healthy CI/test harness.
- Working deterministic input generation for arrays, Top-K, and graphs.
- Correct semantic reference implementations with adversarial tests.
- Robust worker execution and failure reporting.
- Raw measurements and independent confirmation process.
- Practical grid/random/adaptive search.
- Honest typed discovery evidence states, synthetic positive and negative calibration.
- Witness minimization and exact deterministic replay.
- Versioned persistence, CLI, machine-readable reports, and docs.
- Real executed smoke/demo and clean, verified milestone commits.

### Can be trimmed to meet an executable v1 without faking completeness

- Complex web UI, service architecture, remote plugins, Windows support, GPU acceleration, ML models, fancy visualization, distributed operation, exotic statistical models, additional algorithm families, auto-dispatch generation, formal proof pipeline.

### Priorities in a resource-constrained environment

**Correctness > reproducibility > sound discovery gates > executable UX > adaptive-search sophistication > visualization > optional integrations.** If a tradeoff is required, implement a simpler detector that reliably abstains rather than a sophisticated one prone to unsupported discoveries.

---

## 20. FINAL OPERATING INSTRUCTION TO R7

**Operate like an engineering and experimental research team, not a proposal writer.** Inspect → implement → compile → test → execute experiment → capture evidence → adversarially challenge → repair → commit → push where authorized → continue. Preserve outputs and uncertainties faithfully. Do not ask for permission at every milestone. Do not assume a title such as "production" or "research-grade" is earned by the absence of obvious failures. The user supplies only `TARGET_REPO_URL`; make the rest reproducible and automatic.

**End of specification.**
