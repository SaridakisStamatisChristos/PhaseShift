# PhaseShift v3 adversarial falsification ledger

## F-001 — Equal ceilings are not equal consumed budget
Observed in v2 architecture: the dual-ceiling runner pauses after one case, but its old summary is per-run and does not give a complete discovery success-by-consumption curve. V3-01 must reject claims of equal resource consumption based solely on equal limits. **State:** OPEN, explicit regression test required.

## F-002 — False speed promotion from wrong answers
Status must never be represented as a finite winning speed. V3-01 independently rejects failed rows as valid confirmed discoveries. **State:** OPEN.

## F-003 — Synthetic threshold treated as independent real-world discovery
Calibration threshold 24 is explicitly planted by the existing v2 generator. Its truth is suitable for benchmark self-tests, not evidence of novel real-world findings. **State:** OPEN.

## F-004 — Retrofitting holdout after policy tuning
Training/evaluation manifests must be distinct before selecting policy weights; exact manifest hashes and frozen seeds must be persisted. **State:** OPEN.

## F-005 — Process-tree accounting gaps
Linux waited-child RUSAGE includes reaped descendants but not necessarily detached subprocesses; any v3 source must label this limitation rather than claim complete worker CPU. **State:** OPEN.

Update each entry with exact SHA, reproduction command, actual results and repair. Do not delete failed attacks.
