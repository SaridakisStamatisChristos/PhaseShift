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
