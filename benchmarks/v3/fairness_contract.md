# PhaseShift v3 — real CLI diagnostic fairness contract (V3-01)

The training and evaluation manifests are checked-in disjoint seed inventories. Changes to those manifests require a new reviewed protocol revision. V3-01 intentionally tests **existing** v1/v2 incumbent policies, not an unimplemented v3 algorithm. Neither its development nor its evaluation matrix is yet representative of the full V3-01 scientific scenario contract.

Run an inexpensive, explicitly ineligible smoke diagnostic:

```bash
python3 benchmarks/v3/run_internal.py --binary build/phaseshift --out build/v3_quick --suite development --diagnostic
python3 benchmarks/v3/scorecard.py --root build/v3_quick --output build/v3_quick/scorecard.json
```

To execute all 20 preregistered evaluation seeds, omit `--diagnostic` and use a **fresh** `--out` directory and `--suite evaluation`. Do **not** tune a policy based on evaluation results, reinterpret a diagnostic smoke as a holdout, or re-run and select convenient subsets.

Every policy is run through real `explore --pause-after 1`, `resume`, and conditionally `confirm` commands using the existing v2 adapter's real worker and SQLite. Policy order is deterministic-randomized by fixture and seed. Per-command JSONL logs and full run.sqlite files are hashed. An independent scorecard verifies complete inventories, accounting identities, exact persisted hypothesis/seal/status and raw log digest, and computes success-by-**observed** CPU/wall budget curves, including all non-detections and failures.

**Known truth:** single synthetic planted threshold at x=24 and constant-null fixture. All other structures and broader landscapes in the master architecture are **not implemented by this initial diagnostic**.

**Accounting:** observed waited CLI-subtree CPU (including the CLI and workers) and Python harness CPU are distinct; *pure worker CPU* is not isolated. Both ceilings apply at complete case boundaries. Confirmations after the cap do not count as successes; overshoots and incomplete executions remain visible. The terminal known-truth confirmation time is conservatively assigned to the full completed trial. Equal cap != equal consumption, and case counts are a separate estimand.

**Claim:** diagnostic only. The 20 seeds are a floor, not calibrated 5% FWER proof. No external competitors and no benchmark-driven SOTA determination yet. Large `run.sqlite`/raw results belong in retained CI artifacts, never committed to git.

**SQLite evidence seal repair:** the read-only SQLite backup pager snapshot SHA-256
(`sqlite_snapshot_sha256`) includes all committed WAL pages. Raw `run.sqlite`
file-byte hashes are not stable across WAL checkpoints and can omit committed
rows; the adversarial WAL→checkpoint regression in
`tests/v3/benchmark_fairness.py` enforces the corrected logical digest.
Snapshots are ephemeral and never replace authoritative `run.sqlite`.


## V3-04/05 paired-policy development comparison

The distinct frozen `paired_development_manifest.json` compares v14 and v15 on four mixed-dimensional workloads (two synthetic truth-labelled tests and two unlabelled real algorithm families). Seeds 2101–2120 are development-only and disjoint from V3-01 evaluation seeds. Fixed-count trials have 12 fresh matched pairs; the configuration and pair count are identical between policies. Reproducible policy order is randomized by seed/fixture. Routine CI exercises the one-seed eight-trial diagnostic and retains full raw evidence. Run the full 160-trial development suite only in a fresh directory:

```bash
python3 benchmarks/v3/run_paired_development.py --binary build/phaseshift_v3_pairs --out build/v3_paired_dev
python3 benchmarks/v3/run_paired_development.py --out build/v3_paired_dev --verify-only
```

Append `--diagnostic` to the first command for the frozen eight-trial smoke. Trial successes indicate discovery nominations of planted positive edges, not confirmed hypotheses; sorting/Top-K have no labelled truth. The scorecard includes failure and over-ceiling denominators, raw worker CPU, pair measurement wall, waited CLI-subtree + harness invocation CPU and elapsed search wall. It checks consumed ceilings after the complete trial; these are eligibility thresholds, **not** enforced mid-run CPU admission. Verification costs are logged separately. Terminal-time discovery curves are deliberately conservative. Equal pair counts are not equal resource usage. Cost enabled-pair counts expose whether v15 actually used the empirical penalty. No external competitiveness, calibration-coverage probability, held-out promotion or SOTA claim follows from this development-only experiment.
