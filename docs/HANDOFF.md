# PhaseShift session handoff

**Purpose:** when the owner says "continue development", read this file first, verify the state below, then run **Next task**. Update this file at the end of every session, together with `docs/v3/IMPLEMENTATION_LEDGER.md`.

**Last updated:** 2026-11-23, after the v3 merge.

## 1. Where things stand

- **Branch:** work directly on `main`; the owner gave this instruction after v3 was merged. Do not create feature branches unless asked.
- **Head at handoff:** `main` = `9792e81` (after merge commit `e7169dd` of PR #6). All CI green: GCC/Clang 123/123, ASan/UBSan 123/123, production line coverage 95.60%, plus historical-compatibility, multidim, scientific-benchmark and external-confirmation smoke.
- **v3 outcome:** see the final verdict in [`docs/v3/V3_CLAIM_AUDIT.md`](v3/V3_CLAIM_AUDIT.md) and directive §21.
  - Engineering is verified.
  - The one-time external confirmation against SMAC3 2.4.1 and Optuna 4.6.0 TPE found **no advantage**.
  - **No SOTA claim.** External competitiveness is unproven.
- **Authority order:** `docs/PHASESHIFT_SPEC.md` first, then the v3 directive, then `AGENTS.md`. Evidence comes only from source, tests, Git history and CI (see `CLAUDE.md`).

## 2. Session start checklist (do this before changing code)

1. `git status -sb`: confirm you are on `main`, clean and in sync with `origin/main`. Investigate any unexpected local edits; do not discard them.
2. `gh run list --branch main --limit 10`: confirm the head's CI is green. If it is red, fixing it is the next task.
3. Read the tail of `docs/v3/IMPLEMENTATION_LEDGER.md` and this file's **Next task**.
4. **Local verification.** This Windows machine has no C++ toolchain, and the checkout uses CRLF (`core.autocrlf=true`). Build and test in Docker `ubuntu:24.04`:
   - copy the repo into the container;
   - run `git -c core.autocrlf=false reset --hard`;
   - overlay uncommitted files with CR stripped (never under `docs/v3/external_evidence/`, which is `-text`).

   Without this, shell scripts fail (`pipefail\r`) and every git-blob pin check fails. CI checks out LF and is unaffected.

## 3. Hard rules that bind every session

- **Never reuse opened cohorts:** 7101–7120 (V18 sealed) and 51001–51020 (external confirmation). Never re-dispatch `v18-sealed-evaluation` or the cohort opening of `v3-external-confirmation`; the workflow guard refuses anyway.
- **Seeds already used anywhere.** A new cohort must be disjoint from all of these. Verify mechanically with `run_external_confirmation.repository_seed_lists` and `derived_seeds`.
  - plain seed blocks: 1001–1020, 2101–2120, 3001–3020, 4101–4120, 5101–5120, 6101–6120, 6301–6320, 7101–7120, 32001–32020, 33001–33020, 50500, 51001–51020, 84001–84032, 95001–98064, 100001–101064;
  - derived external query seeds: 132001–144020 and 150500–162020 (`S+100000+1000n`, n<12);
  - derived confirmation seeds: `10^9+10S+j`.
- **Frozen evidence stays frozen.** Never edit `docs/v3/external_evidence/*`, `benchmarks/v3/external_confirmation.json` or any other frozen manifest or sealed artifact. New protocols get new versioned files.
- **Leave the development study runners alone.** `run_real_external.py` and `run_real_smac.py` intentionally fail at HEAD, because they are pinned to the pre-holdout V18 source. Do not edit them; new studies get new runners that *import* their adapters (as `run_external_confirmation.py` does).
- **Gates.** Each commit must be atomic and tested, and pushed only after it is verified. Never weaken a test to pass. Report only what actually ran; negative results are recorded, not hidden.
- **Verify every claim.** Freeze hypotheses before any confirmation. A confirmation is one-time, on a fresh derived-seed-disjoint cohort, and needs the owner's explicit go-ahead before it is opened.

## 4. Next task

**V4-00: known-truth external benchmark on the v3 thesis endpoint (development only).**

Why this is first: everything external so far tested "fresh-seed reproducibility of the top-2 nominated edges on unlabelled workloads". That endpoint rewards finding the single largest effect, which is what SMAC and TPE optimise, and it saturated on `sort_ascending`. The v3 thesis (directive §2.3) is a different one: **distinct ground-truth boundaries confirmed per unit of consumed CPU**, `S_i(b)` and `Y_i(B)`. It has never been tested against external competitors. Full rationale is in [`docs/v3/SOTA_ROADMAP.md`](v3/SOTA_ROADMAP.md).

Build, in a new directory `benchmarks/v4/`, with no change to frozen v3 files:

1. **Manifest.** `known_truth_external_development.json`, the development suite.
   - **Fixtures:** the `benchmarks/v3/multidim_truth.py` families (multi-context steps, sparse multi-step, rare hidden cliff, masked interactions) plus the null controls (`smooth_no_jump`, `multidim_null`).
   - **Seeds:** a **new** 20-seed cohort, proven disjoint in declared and derived space.
   - **Policies:** V18 (`acquisition-v3-coverage`), v14 `conditional-tree`, v15 `acquisition-v3`, SMAC3 2.4.1, Optuna 4.6.0 TPE and random.
2. **Runner.** It must use the same legal-edge domain for every policy, enumerated from `multidim_truth.domain` with conditional contexts kept.
   - Each policy stops at a **matched consumed worker-CPU budget** `B`, not a query count. Record query counts, optimizer and harness overhead, and wall separately.
   - The SMAC, TPE and random adapters are imported from the v3 runners, so they are genuine and pinned.
3. **Endpoints**, frozen in the manifest before running:
   - primary: AUC of `S_i(b)` over a predeclared CPU interval;
   - secondary: `Y_i(B)` distinct true boundaries, false nominations on null fixtures, and localization error;
   - matching is one-to-one through `multidim_truth.classify`.
4. **Aggregator.** It must independently re-audit the native SQLite evidence, match against truth, compute paired seed-cluster bootstrap intervals, and reproduce its scorecard byte for byte with `--verify-only`.
5. **Tests and CI.**
   - CTest contract test: truth exactness, budget accounting, cohort freshness, and rejection of tampered manifest or summary edits.
   - New workflow: push runs the contract checks and a single-seed diagnostic; `workflow_dispatch` runs all 20 development seeds.

**Acceptance:** CI green on `main`. A development scorecard is recorded in the ledger. Write a short decision note: either V18 shows a development advantage on `S(b)`/`Y(B)` worth freezing into a confirmation, or it does not, and the next task becomes algorithm work (roadmap §3). Do **not** open any confirmation cohort without the owner's explicit approval.

## 5. Task queue after V4-00

These come from [`docs/v3/SOTA_ROADMAP.md`](v3/SOTA_ROADMAP.md). Re-order them by the V4-00 outcome.

1. If V4-00 shows no advantage: algorithm work on the measured weakness, with ablations (roadmap §3).
2. Matched-CPU fairness for cheap workers: a persistent-worker or amortized-startup option, so per-query process startup does not dominate.
3. V3-02 coordinator extraction (engineering debt; needed to promote any policy into the main `explore` CLI).
4. Complete accounting: an optional cgroup v2 harness where available, and hardening of the v1/v2 store (integrity check on open).
5. Pre-registered confirmation of any development advantage: new cohort, owner approval, one-time.
6. Multi-host / repeated-session replication before any public claim.

## 6. End-of-session duties

- Append a dated entry to `docs/v3/IMPLEMENTATION_LEDGER.md` (and `FALSIFICATION_LEDGER.md` for new attacks or defects).
- Update §1 and §4 of this file: new head SHA, CI status, and the next concrete task.
- Report what changed, the tests actually run, measured differences, commit SHA and CI status, unresolved issues and the next step.
