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
